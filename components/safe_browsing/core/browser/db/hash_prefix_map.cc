// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"

#include "base/files/file_util.h"
#include "base/strings/strcat.h"

namespace safe_browsing {
namespace {

std::string GenerateExtension(PrefixSize size) {
  return base::StrCat(
      {base::NumberToString(size), "_",
       base::NumberToString(
           base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds())});
}

}  // namespace

InMemoryHashPrefixMap::InMemoryHashPrefixMap() = default;
InMemoryHashPrefixMap::~InMemoryHashPrefixMap() = default;

void InMemoryHashPrefixMap::Clear() {
  map_.clear();
}

HashPrefixMapView InMemoryHashPrefixMap::view() const {
  HashPrefixMapView view;
  for (const auto& kv : map_)
    view.emplace(kv.first, kv.second);
  return view;
}

void InMemoryHashPrefixMap::Append(PrefixSize size, HashPrefixesView prefix) {
  map_[size].append(prefix.data(), prefix.size());
}

void InMemoryHashPrefixMap::Reserve(PrefixSize size, size_t capacity) {
  map_[size].reserve(capacity);
}

ApplyUpdateResult InMemoryHashPrefixMap::ReadFromDisk(
    const V4StoreFileFormat& file_format) {
  // This is currently handled in V4Store::UpdateHashPrefixMapFromAdditions().
  // TODO(cduvall): Move that logic here?
  DCHECK(file_format.hash_files().empty());
  return APPLY_UPDATE_SUCCESS;
}

bool InMemoryHashPrefixMap::WriteToDisk(V4StoreFileFormat* file_format) {
  ListUpdateResponse* lur = file_format->mutable_list_update_response();
  for (const auto& entry : map_) {
    ThreatEntrySet* additions = lur->add_additions();
    // TODO(vakh): Write RICE encoded hash prefixes on disk. Not doing so
    // currently since it takes a long time to decode them on startup, which
    // blocks resource load. See: http://crbug.com/654819
    additions->set_compression_type(RAW);
    additions->mutable_raw_hashes()->set_prefix_size(entry.first);
    additions->mutable_raw_hashes()->set_raw_hashes(entry.second);
  }
  return true;
}

ApplyUpdateResult InMemoryHashPrefixMap::IsValid() const {
  return APPLY_UPDATE_SUCCESS;
}

HashPrefixMap::MigrateResult InMemoryHashPrefixMap::MigrateFileFormat(
    const base::FilePath& store_path,
    V4StoreFileFormat* file_format) {
  if (file_format->hash_files().empty())
    return MigrateResult::kNotNeeded;

  ListUpdateResponse* lur = file_format->mutable_list_update_response();
  for (const auto& hash_file : file_format->hash_files()) {
    std::string contents;
    base::FilePath hashes_path =
        MmapHashPrefixMap::GetPath(store_path, hash_file.extension());
    if (!base::ReadFileToStringWithMaxSize(hashes_path, &contents,
                                           kMaxStoreSizeBytes)) {
      return MigrateResult::kFailure;
    }
    auto* additions = lur->add_additions();
    additions->set_compression_type(RAW);
    additions->mutable_raw_hashes()->set_prefix_size(hash_file.prefix_size());
    additions->mutable_raw_hashes()->set_raw_hashes(std::move(contents));
  }
  file_format->clear_hash_files();
  return MigrateResult::kSuccess;
}

// Writes a hash prefix file, and buffers writes to avoid a write call for each
// hash prefix. The file will be deleted if Finish() is never called.
class MmapHashPrefixMap::BufferedFileWriter {
 public:
  BufferedFileWriter(const base::FilePath& store_path,
                     PrefixSize prefix_size,
                     size_t buffer_size)
      : extension_(GenerateExtension(prefix_size)),
        path_(GetPath(store_path, extension_)),
        buffer_size_(buffer_size),
        file_(path_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE),
        has_error_(!file_.IsValid()) {
    buffer_.reserve(buffer_size);
  }

  ~BufferedFileWriter() {
    // File was never finished, delete now.
    if (file_.IsValid() || has_error_) {
      file_.Close();
      base::DeleteFile(path_);
    }
  }

  void Write(HashPrefixesView data) {
    if (has_error_)
      return;

    if (buffer_.size() + data.size() >= buffer_size_)
      Flush();

    if (data.size() > buffer_size_)
      WriteToFile(data);
    else
      buffer_.append(data.data(), data.size());
  }

  bool Finish() {
    Flush();
    file_.Close();
    return !has_error_;
  }

  const std::string& extension() const { return extension_; }

 private:
  void Flush() {
    WriteToFile(buffer_);
    buffer_.clear();
  }

  void WriteToFile(HashPrefixesView data) {
    if (has_error_ || data.empty())
      return;

    if (!file_.WriteAtCurrentPosAndCheck(base::as_bytes(base::make_span(data))))
      has_error_ = true;
  }

  const std::string extension_;
  const base::FilePath path_;
  const size_t buffer_size_;
  base::File file_;
  std::string buffer_;
  bool has_error_;
};

MmapHashPrefixMap::MmapHashPrefixMap(const base::FilePath& store_path,
                                     size_t buffer_size)
    : store_path_(store_path), buffer_size_(buffer_size) {}
MmapHashPrefixMap::~MmapHashPrefixMap() = default;

void MmapHashPrefixMap::Clear() {
  map_.clear();
}

HashPrefixMapView MmapHashPrefixMap::view() const {
  HashPrefixMapView view;
  for (const auto& kv : map_) {
    if (!kv.second.IsReadable())
      continue;

    view.emplace(kv.first, kv.second.GetView());
  }
  return view;
}

void MmapHashPrefixMap::Append(PrefixSize size, HashPrefixesView prefix) {
  if (prefix.empty())
    return;

  GetFileInfo(size).GetOrCreateWriter(buffer_size_)->Write(prefix);
}

void MmapHashPrefixMap::Reserve(PrefixSize size, size_t capacity) {}

ApplyUpdateResult MmapHashPrefixMap::ReadFromDisk(
    const V4StoreFileFormat& file_format) {
  DCHECK(file_format.list_update_response().additions().empty());
  for (const auto& hash_file : file_format.hash_files()) {
    if (!GetFileInfo(hash_file.prefix_size()).Initialize(hash_file))
      return MMAP_FAILURE;
  }
  return APPLY_UPDATE_SUCCESS;
}

bool MmapHashPrefixMap::WriteToDisk(V4StoreFileFormat* file_format) {
  for (auto& [size, file_info] : map_) {
    auto* hash_file = file_format->add_hash_files();
    if (!file_info.Finalize(hash_file))
      return false;

    if (!file_info.Initialize(*hash_file))
      return false;
  }
  return true;
}

ApplyUpdateResult MmapHashPrefixMap::IsValid() const {
  for (const auto& kv : map_) {
    if (!kv.second.IsReadable())
      return MMAP_FAILURE;
  }
  return APPLY_UPDATE_SUCCESS;
}

HashPrefixMap::MigrateResult MmapHashPrefixMap::MigrateFileFormat(
    const base::FilePath& store_path,
    V4StoreFileFormat* file_format) {
  ListUpdateResponse* lur = file_format->mutable_list_update_response();
  if (lur->additions().empty())
    return MigrateResult::kNotNeeded;

  for (auto& addition : *lur->mutable_additions()) {
    Append(addition.raw_hashes().prefix_size(),
           addition.raw_hashes().raw_hashes());
  }
  lur->clear_additions();
  return MigrateResult::kSuccess;
}

// static
base::FilePath MmapHashPrefixMap::GetPath(const base::FilePath& store_path,
                                          const std::string& extension) {
  return store_path.AddExtensionASCII(extension);
}

const std::string& MmapHashPrefixMap::GetExtensionForTesting(PrefixSize size) {
  return GetFileInfo(size).GetExtensionForTesting();  // IN-TEST
}

MmapHashPrefixMap::FileInfo& MmapHashPrefixMap::GetFileInfo(PrefixSize size) {
  auto [it, inserted] = map_.try_emplace(size, store_path_, size);
  return it->second;
}

MmapHashPrefixMap::FileInfo::FileInfo(const base::FilePath& store_path,
                                      PrefixSize size)
    : store_path_(store_path), prefix_size_(size) {}

MmapHashPrefixMap::FileInfo::~FileInfo() = default;

HashPrefixesView MmapHashPrefixMap::FileInfo::GetView() const {
  DCHECK(IsReadable());
  return HashPrefixesView(reinterpret_cast<const char*>(file_.data()),
                          file_.length());
}

bool MmapHashPrefixMap::FileInfo::Initialize(const HashFile& hash_file) {
  return file_.Initialize(GetPath(store_path_, hash_file.extension()));
}

bool MmapHashPrefixMap::FileInfo::Finalize(HashFile* hash_file) {
  if (!writer_->Finish())
    return false;

  hash_file->set_prefix_size(prefix_size_);
  hash_file->set_extension(writer_->extension());
  writer_.reset();
  return true;
}

MmapHashPrefixMap::BufferedFileWriter*
MmapHashPrefixMap::FileInfo::GetOrCreateWriter(size_t buffer_size) {
  DCHECK(!file_.IsValid());
  if (!writer_) {
    writer_ = std::make_unique<BufferedFileWriter>(store_path_, prefix_size_,
                                                   buffer_size);
  }
  return writer_.get();
}

const std::string& MmapHashPrefixMap::FileInfo::GetExtensionForTesting() const {
  return writer_->extension();
}

}  // namespace safe_browsing
