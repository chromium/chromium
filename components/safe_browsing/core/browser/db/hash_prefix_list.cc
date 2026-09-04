// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_list.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"

namespace safe_browsing {

namespace {

std::string GenerateExtension() {
  return base::NumberToString(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

class HashPrefixListWriteSession : public HashPrefixContainer::WriteSession {
 public:
  HashPrefixListWriteSession() = default;
  HashPrefixListWriteSession(const HashPrefixListWriteSession&) = delete;
  HashPrefixListWriteSession& operator=(const HashPrefixListWriteSession&) =
      delete;
  ~HashPrefixListWriteSession() override = default;
};

}  // namespace

HashPrefixList::HashPrefixList(
    const base::FilePath& store_path,
    PrefixSize prefix_size,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t buffer_size)
    : HashPrefixContainer(store_path, std::move(task_runner), buffer_size),
      prefix_size_(prefix_size) {}

HashPrefixList::~HashPrefixList() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
}

void HashPrefixList::Clear() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Clear the list on the db task runner, since the memory mapped files
    // should be destroyed on the same thread they were created.
    // base::Unretained is safe since the list is destroyed on the db task
    // runner.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&HashPrefixList::ClearOnTaskRunner,
                                          base::Unretained(this)));
  } else {
    ClearOnTaskRunner();
  }
}

void HashPrefixList::ClearOnTaskRunner() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  file_info_.reset();
}

HashPrefixMapView HashPrefixList::view() const {
  HashPrefixMapView view;
  HashPrefixesView raw_view = GetRawView();
  if (!raw_view.empty()) {
    view.emplace(prefix_size_, raw_view);
  }
  return view;
}

HashPrefixesView HashPrefixList::GetRawView() const {
  if (file_info_ && file_info_->IsReadable()) {
    return file_info_->GetView();
  }
  return HashPrefixesView();
}

void HashPrefixList::Append(PrefixSize size, HashPrefixesView prefix) {
  if (prefix.empty()) {
    return;
  }

  CHECK_EQ(size, prefix_size_);
  GetFileInfo()
      .GetOrCreateWriter(buffer_size_, GenerateExtension(), "V5Store")
      ->Write(prefix);
}

V5ApplyUpdateResult HashPrefixList::ReadFromDisk(
    const SBStoreFileFormat& file_format) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const V5StoreFileFormat& v5_file_format = file_format.v5_file_format();
  const V5HashFile& hash_file = v5_file_format.list_details().hash_file();

  if (hash_file.file_size() == 0) {
    return V5ApplyUpdateResult::kSuccess;
  }

  if (hash_file.file_size() % prefix_size_ != 0) {
    return V5ApplyUpdateResult::kFileSizeNotMultipleOfPrefixSize;
  }

  auto& file_info = GetFileInfo();
  if (hash_file.extension().empty()) {
    return V5ApplyUpdateResult::kMmapFailure;
  }
  if (!file_info.Initialize(hash_file.extension(), hash_file.file_size(),
                            /*initialize_after_write=*/false, "V5Store")) {
    return V5ApplyUpdateResult::kMmapFailure;
  }
  return V5ApplyUpdateResult::kSuccess;
}

std::unique_ptr<HashPrefixContainer::WriteSession> HashPrefixList::WriteToDisk(
    SBStoreFileFormat& file_format) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  V5StoreFileFormat* v5_file_format = file_format.v5_file_format_mutable();

  if (!file_info_) {
    return std::make_unique<HashPrefixListWriteSession>();
  }

  std::optional<FinalizedFileInfo> finalized_info =
      file_info_->Finalize("V5Store");
  if (!finalized_info.has_value()) {
    return nullptr;
  }

  if (finalized_info->file_size == 0) {
    return std::make_unique<HashPrefixListWriteSession>();
  }

  if (!file_info_->Initialize(finalized_info->extension,
                              finalized_info->file_size,
                              /*initialize_after_write=*/true, "V5Store")) {
    return nullptr;
  }

  V5HashFile hash_file;
  hash_file.set_file_size(finalized_info->file_size);
  hash_file.set_extension(std::move(finalized_info->extension));

  v5_file_format->mutable_list_details()->mutable_hash_file()->Swap(&hash_file);

  return std::make_unique<HashPrefixListWriteSession>();
}

V5ApplyUpdateResult HashPrefixList::IsValid() const {
  if (file_info_ && !file_info_->IsReadable()) {
    return V5ApplyUpdateResult::kMmapFailure;
  }

  return V5ApplyUpdateResult::kSuccess;
}

HashPrefixStr HashPrefixList::GetMatchingHashPrefix(
    std::string_view full_hash) {
  if (file_info_) {
    HashPrefixStr matching_prefix = file_info_->Matches(full_hash);
    if (!matching_prefix.empty()) {
      return matching_prefix;
    }
  }
  return HashPrefixStr();
}

void HashPrefixList::GetPrefixInfo(
    google::protobuf::RepeatedPtrField<
        DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>* prefix_sets) {
  if (file_info_) {
    DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet* prefix_set =
        prefix_sets->Add();
    prefix_set->set_size(prefix_size_);
    prefix_set->set_count(file_info_->GetView().size() / prefix_size_);
  }
}

std::vector<base::FilePath> HashPrefixList::GetPaths() const {
  std::vector<base::FilePath> paths;
  if (file_info_ && file_info_->IsReadable()) {
    paths.push_back(file_info_->GetPath());
  }
  return paths;
}

HashPrefixList::FileInfo& HashPrefixList::GetFileInfo() {
  if (!file_info_) {
    file_info_ = std::make_unique<FileInfo>(store_path_, prefix_size_);
  }
  return *file_info_;
}

const std::string& HashPrefixList::GetExtensionForTesting() {
  return GetFileInfo().GetExtensionForTesting();  // IN-TEST
}

}  // namespace safe_browsing
