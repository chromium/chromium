// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"

#include <optional>
#include <string_view>

#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/safe_browsing/core/browser/db/prefix_iterator.h"
#include "components/safe_browsing/core/common/features.h"

namespace safe_browsing {
namespace {

std::string GenerateExtension(PrefixSize size) {
  return base::StrCat(
      {base::NumberToString(size), "_",
       base::NumberToString(
           base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds())});
}

// Returns true if |hash_prefix| with PrefixSize |size| exists in |prefixes|.
bool HashPrefixMatches(std::string_view prefix,
                       HashPrefixesView prefixes,
                       PrefixSize size,
                       size_t start,
                       size_t end) {
  return std::binary_search(PrefixIterator(prefixes, start, size),
                            PrefixIterator(prefixes, end, size), prefix);
}

}  // namespace

// Writes a hash prefix file, and buffers writes to avoid a write call for each
// hash prefix. The file will be deleted if Finish() is never called.
class HashPrefixMap::BufferedFileWriter {
 public:
  BufferedFileWriter(const base::FilePath& store_path,
                     PrefixSize prefix_size,
                     size_t buffer_size)
      : extension_(GenerateExtension(prefix_size)),
        path_(GetPath(store_path, extension_)),
        prefix_size_(prefix_size),
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

    cur_size_ += data.size();

    if (buffer_.size() + data.size() >= buffer_size_)
      Flush();

    if (data.size() > buffer_size_)
      WriteToFile(data);
    else
      buffer_.append(data);
  }

  bool Finish() {
    Flush();
    file_.Close();
    return !has_error_ && cur_size_ % prefix_size_ == 0;
  }

  size_t GetFileSize() const { return cur_size_; }

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
  const size_t prefix_size_;
  const size_t buffer_size_;
  size_t cur_size_ = 0;
  base::File file_;
  std::string buffer_;
  bool has_error_;
};

HashPrefixMap::HashPrefixMap(
    const base::FilePath& store_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t buffer_size)
    : store_path_(store_path),
      task_runner_(task_runner
                       ? std::move(task_runner)
                       : base::SequencedTaskRunner::GetCurrentDefault()),
      buffer_size_(buffer_size) {}

HashPrefixMap::~HashPrefixMap() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void HashPrefixMap::Clear() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Clear the map on the db task runner, since the memory mapped files should
    // be destroyed on the same thread they were created. base::Unretained is
    // safe since the map is destroyed on the db task runner.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&HashPrefixMap::ClearOnTaskRunner,
                                          base::Unretained(this)));
  } else {
    map_.clear();
  }
}

void HashPrefixMap::ClearOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  map_.clear();
}

HashPrefixMapView HashPrefixMap::view() const {
  HashPrefixMapView view;
  view.reserve(map_.size());
  for (const auto& kv : map_) {
    if (!kv.second.IsReadable())
      continue;

    view.emplace(kv.first, kv.second.GetView());
  }
  return view;
}

HashPrefixesView HashPrefixMap::at(PrefixSize size) const {
  const FileInfo& info = map_.at(size);
  CHECK(info.IsReadable());
  return info.GetView();
}

void HashPrefixMap::Append(PrefixSize size, HashPrefixesView prefix) {
  if (prefix.empty())
    return;

  GetFileInfo(size).GetOrCreateWriter(buffer_size_)->Write(prefix);
}

ApplyUpdateResult HashPrefixMap::ReadFromDisk(
    const V4StoreFileFormat& file_format) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(file_format.list_update_response().additions().empty());
  for (const auto& hash_file : file_format.hash_files()) {
    PrefixSize prefix_size = hash_file.prefix_size();
    if (hash_file.file_size() % prefix_size != 0) {
      return ADDITIONS_SIZE_UNEXPECTED_FAILURE;
    }

    auto& file_info = GetFileInfo(prefix_size);
    if (!file_info.Initialize(hash_file)) {
      return MMAP_FAILURE;
    }
  }
  return APPLY_UPDATE_SUCCESS;
}

namespace {

class HashPrefixMapWriteSession : public HashPrefixMap::WriteSession {
 public:
  HashPrefixMapWriteSession() = default;
  HashPrefixMapWriteSession(const HashPrefixMapWriteSession&) = delete;
  HashPrefixMapWriteSession& operator=(const HashPrefixMapWriteSession&) =
      delete;
  ~HashPrefixMapWriteSession() override = default;
};

}  // namespace

std::unique_ptr<HashPrefixMap::WriteSession> HashPrefixMap::WriteToDisk(
    V4StoreFileFormat* file_format) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  for (auto& [size, file_info] : map_) {
    HashFile hash_file;
    if (!file_info.Finalize(&hash_file)) {
      return nullptr;
    }

    if (hash_file.file_size() == 0) {
      continue;
    }

    if (!file_info.Initialize(hash_file)) {
      return nullptr;
    }

    file_format->add_hash_files()->Swap(&hash_file);
  }
  return std::make_unique<HashPrefixMapWriteSession>();
}

ApplyUpdateResult HashPrefixMap::IsValid() const {
  for (const auto& kv : map_) {
    if (!kv.second.IsReadable())
      return MMAP_FAILURE;
  }
  return APPLY_UPDATE_SUCCESS;
}

HashPrefixStr HashPrefixMap::GetMatchingHashPrefix(std::string_view full_hash) {
  for (const auto& kv : map_) {
    HashPrefixStr matching_prefix = kv.second.Matches(full_hash);
    if (!matching_prefix.empty())
      return matching_prefix;
  }
  return HashPrefixStr();
}

HashPrefixMap::MigrateResult HashPrefixMap::MigrateFileFormat(
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

void HashPrefixMap::GetPrefixInfo(
    google::protobuf::RepeatedPtrField<
        DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>* prefix_sets) {
  for (const auto& size_and_info : map_) {
    const PrefixSize& size = size_and_info.first;
    const FileInfo& info = size_and_info.second;

    DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet* prefix_set =
        prefix_sets->Add();
    prefix_set->set_size(size);
    prefix_set->set_count(info.GetView().size() / size);
  }
}

// static
base::FilePath HashPrefixMap::GetPath(const base::FilePath& store_path,
                                      const std::string& extension) {
  return store_path.AddExtensionASCII(extension);
}

const std::string& HashPrefixMap::GetExtensionForTesting(PrefixSize size) {
  return GetFileInfo(size).GetExtensionForTesting();  // IN-TEST
}

void HashPrefixMap::ClearAndWaitForTesting() {
  Clear();
  base::RunLoop run_loop;
  task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

HashPrefixMap::FileInfo& HashPrefixMap::GetFileInfo(PrefixSize size) {
  auto [it, inserted] = map_.try_emplace(size, store_path_, size);
  return it->second;
}

HashPrefixMap::FileInfo::FileInfo(const base::FilePath& store_path,
                                  PrefixSize size)
    : store_path_(store_path), prefix_size_(size) {}

HashPrefixMap::FileInfo::~FileInfo() = default;

HashPrefixesView HashPrefixMap::FileInfo::GetView() const {
  DCHECK(IsReadable());
  return HashPrefixesView(reinterpret_cast<const char*>(file_.data()),
                          file_.length());
}

bool HashPrefixMap::FileInfo::Initialize(const HashFile& hash_file) {
  // Make sure file size is correct before attempting to mmap.
  base::FilePath path = GetPath(store_path_, hash_file.extension());
  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (!file_size.has_value()) {
    return false;
  }
  if (static_cast<uint64_t>(file_size.value()) != hash_file.file_size()) {
    return false;
  }

  if (IsReadable()) {
    DCHECK_EQ(file_.length(), hash_file.file_size());
    return true;
  }

  if (!file_.Initialize(path)) {
    return false;
  }

  if (file_.length() != static_cast<size_t>(file_size.value())) {
    return false;
  }

  return true;
}

bool HashPrefixMap::FileInfo::Finalize(HashFile* hash_file) {
  if (!writer_->Finish())
    return false;

  hash_file->set_prefix_size(prefix_size_);

  hash_file->set_file_size(writer_->GetFileSize());
  hash_file->set_extension(writer_->extension());
  writer_.reset();
  return true;
}

HashPrefixStr HashPrefixMap::FileInfo::Matches(
    std::string_view full_hash) const {
  HashPrefixStr hash_prefix(full_hash.substr(0, prefix_size_));
  HashPrefixesView prefixes = GetView();

  uint32_t start = 0;
  uint32_t end = prefixes.size() / prefix_size_;

  // If the start is the same as end, the hash doesn't exist.
  if (start == end) {
    return HashPrefixStr();
  }

  // TODO(crbug.com/40062772): Remove crash logging.
  std::string_view start_prefix = prefixes.substr(0, prefix_size_);
  std::string_view end_prefix =
      prefixes.substr(prefix_size_ * (end - 1), prefix_size_);
  SCOPED_CRASH_KEY_STRING64(
      "SafeBrowsing", "prefix_match",
      base::StrCat({base::NumberToString(start), ":", base::NumberToString(end),
                    ":", base::NumberToString(prefix_size_), ":",
                    base::NumberToString(prefixes.size()), ":",
                    base::NumberToString(start_prefix.compare(hash_prefix)),
                    ":",
                    base::NumberToString(end_prefix.compare(hash_prefix))}));

  if (HashPrefixMatches(hash_prefix, prefixes, prefix_size_, start, end))
    return hash_prefix;
  return HashPrefixStr();
}

HashPrefixMap::BufferedFileWriter* HashPrefixMap::FileInfo::GetOrCreateWriter(
    size_t buffer_size) {
  DCHECK(!file_.IsValid());
  if (!writer_) {
    writer_ = std::make_unique<BufferedFileWriter>(store_path_, prefix_size_,
                                                   buffer_size);
  }
  return writer_.get();
}

const std::string& HashPrefixMap::FileInfo::GetExtensionForTesting() const {
  return writer_->extension();
}

}  // namespace safe_browsing
