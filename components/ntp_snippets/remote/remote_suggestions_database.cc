// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_database.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"

using leveldb_proto::ProtoDatabase;
using leveldb_proto::ProtoDatabaseProvider;

namespace {
const char kSnippetDatabaseFolder[] = "snippets";
const char kImageDatabaseFolder[] = "images";

const size_t kDatabaseWriteBufferSizeBytes = 128 << 10;
}  // namespace

namespace ntp_snippets {

RemoteSuggestionsDatabase::RemoteSuggestionsDatabase(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_dir)
    : RemoteSuggestionsDatabase(
          proto_database_provider,
          database_dir,
          base::CreateSequencedTaskRunner(
              {base::ThreadPool(), base::MayBlock(),
               base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

RemoteSuggestionsDatabase::RemoteSuggestionsDatabase(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RemoteSuggestionsDatabase(
          proto_database_provider->GetDB<SnippetProto>(
              leveldb_proto::ProtoDbType::REMOTE_SUGGESTIONS_DATABASE,
              database_dir.AppendASCII(kSnippetDatabaseFolder),
              task_runner),
          proto_database_provider->GetDB<SnippetImageProto>(
              leveldb_proto::ProtoDbType::REMOTE_SUGGESTIONS_IMAGE_DATABASE,
              database_dir.AppendASCII(kImageDatabaseFolder),
              task_runner)) {}

RemoteSuggestionsDatabase::RemoteSuggestionsDatabase(
    std::unique_ptr<ProtoDatabase<SnippetProto>> database,
    std::unique_ptr<ProtoDatabase<SnippetImageProto>> image_database)
    : database_(std::move(database)),
      database_initialized_(false),
      image_database_(std::move(image_database)),
      image_database_initialized_(false) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.reuse_logs = false;  // Consumes less RAM over time.
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;

  database_->Init(options,
                  base::BindOnce(&RemoteSuggestionsDatabase::OnDatabaseInited,
                                 weak_ptr_factory_.GetWeakPtr()));

  image_database_->Init(
      options, base::BindOnce(&RemoteSuggestionsDatabase::OnImageDatabaseInited,
                              weak_ptr_factory_.GetWeakPtr()));
}

RemoteSuggestionsDatabase::~RemoteSuggestionsDatabase() = default;

bool RemoteSuggestionsDatabase::IsInitialized() const {
  return !IsErrorState() && database_initialized_ &&
         image_database_initialized_;
}

bool RemoteSuggestionsDatabase::IsErrorState() const {
  return !database_ || !image_database_;
}

void RemoteSuggestionsDatabase::SetErrorCallback(
    const base::Closure& error_callback) {
  error_callback_ = error_callback;
}

void RemoteSuggestionsDatabase::LoadSnippets(SnippetsCallback callback) {
  if (IsInitialized()) {
    LoadSnippetsImpl(std::move(callback));
  } else {
    pending_snippets_callbacks_.emplace_back(std::move(callback));
  }
}

void RemoteSuggestionsDatabase::SaveSnippet(const RemoteSuggestion& snippet) {
  if (IsErrorState()) {
    DVLOG(0) << "Attempted save snippet but db is in an error state, aborting";
    return;
  }

  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  // OnDatabaseLoaded relies on the detail that the primary snippet id goes
  // first in the protocol representation.
  DCHECK_EQ(snippet.ToProto().ids(0), snippet.id());
  entries_to_save->emplace_back(snippet.id(), snippet.ToProto());
  SaveSnippetsImpl(std::move(entries_to_save));
}

void RemoteSuggestionsDatabase::SaveSnippets(
    const RemoteSuggestion::PtrVector& snippets) {
  if (IsErrorState()) {
    DVLOG(0) << "Attempted save snippets but db is in an error state, aborting";
    return;
  }

  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  for (const std::unique_ptr<RemoteSuggestion>& snippet : snippets) {
    // OnDatabaseLoaded relies on the detail that the primary snippet id goes
    // first in the protocol representation.
    DCHECK_EQ(snippet->ToProto().ids(0), snippet->id());
    entries_to_save->emplace_back(snippet->id(), snippet->ToProto());
  }
  SaveSnippetsImpl(std::move(entries_to_save));
}

void RemoteSuggestionsDatabase::DeleteSnippet(const std::string& snippet_id) {
  DeleteSnippets(std::make_unique<std::vector<std::string>>(1, snippet_id));
}

void RemoteSuggestionsDatabase::DeleteSnippets(
    std::unique_ptr<std::vector<std::string>> snippet_ids) {
  if (IsErrorState()) {
    DVLOG(0)
        << "Attempted delete snippets but db is in an error state, aborting";
    return;
  }

  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  database_->UpdateEntries(
      std::move(entries_to_save), std::move(snippet_ids),
      base::BindOnce(&RemoteSuggestionsDatabase::OnDatabaseSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::LoadImage(const std::string& snippet_id,
                                          SnippetImageCallback callback) {
  if (IsInitialized()) {
    LoadImageImpl(snippet_id, std::move(callback));
  } else {
    pending_image_callbacks_.emplace_back(snippet_id, std::move(callback));
  }
}

void RemoteSuggestionsDatabase::SaveImage(const std::string& snippet_id,
                                          const std::string& image_data) {
  if (IsErrorState()) {
    DVLOG(0) << "Attempted save image but db is in an error state, aborting";
    return;
  }

  SnippetImageProto image_proto;
  image_proto.set_data(image_data);

  std::unique_ptr<ImageKeyEntryVector> entries_to_save(
      new ImageKeyEntryVector());
  entries_to_save->emplace_back(snippet_id, std::move(image_proto));

  image_database_->UpdateEntries(
      std::move(entries_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&RemoteSuggestionsDatabase::OnImageDatabaseSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::DeleteImage(const std::string& snippet_id) {
  DeleteImages(std::make_unique<std::vector<std::string>>(1, snippet_id));
}

void RemoteSuggestionsDatabase::DeleteImages(
    std::unique_ptr<std::vector<std::string>> snippet_ids) {
  if (IsErrorState()) {
    DVLOG(0) << "Attempted delete images but db is in an error state, aborting";
    return;
  }
  image_database_->UpdateEntries(
      std::make_unique<ImageKeyEntryVector>(), std::move(snippet_ids),
      base::BindOnce(&RemoteSuggestionsDatabase::OnImageDatabaseSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::GarbageCollectImages(
    std::unique_ptr<std::set<std::string>> alive_snippet_ids) {
  if (IsErrorState()) {
    DVLOG(0) << "Attempted gc but db is in an error state, aborting";
    return;
  }
  image_database_->LoadKeys(base::BindOnce(
      &RemoteSuggestionsDatabase::DeleteUnreferencedImages,
      weak_ptr_factory_.GetWeakPtr(), std::move(alive_snippet_ids)));
}

void RemoteSuggestionsDatabase::OnDatabaseInited(
    leveldb_proto::Enums::InitStatus status) {
  DCHECK(!database_initialized_);
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    DVLOG(1) << "RemoteSuggestionsDatabase init failed.";
    OnDatabaseError();
    return;
  }
  database_initialized_ = true;
  if (IsInitialized()) {
    ProcessPendingLoads();
  }
}

void RemoteSuggestionsDatabase::OnDatabaseLoaded(
    SnippetsCallback callback,
    bool success,
    std::unique_ptr<std::vector<SnippetProto>> entries) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase load failed.";
    OnDatabaseError();
    return;
  }

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  RemoteSuggestion::PtrVector snippets;
  for (const SnippetProto& proto : *entries) {
    std::unique_ptr<RemoteSuggestion> snippet =
        RemoteSuggestion::CreateFromProto(proto);
    if (snippet) {
      snippets.emplace_back(std::move(snippet));
    } else {
      if (proto.ids_size() > 0) {
        LOG(WARNING) << "Invalid proto from DB " << proto.ids(0);
        keys_to_remove->emplace_back(proto.ids(0));
      } else {
        LOG(WARNING)
            << "Loaded proto without ID from the DB. Cannot clean this up.";
      }
    }
  }

  std::move(callback).Run(std::move(snippets));

  // If any of the snippet protos couldn't be converted to actual snippets,
  // clean them up now.
  if (!keys_to_remove->empty()) {
    DeleteSnippets(std::move(keys_to_remove));
  }
}

void RemoteSuggestionsDatabase::OnDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase save failed.";
    OnDatabaseError();
  }
}

void RemoteSuggestionsDatabase::OnImageDatabaseInited(
    leveldb_proto::Enums::InitStatus status) {
  DCHECK(!image_database_initialized_);
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    DVLOG(1) << "RemoteSuggestionsDatabase init failed.";
    OnDatabaseError();
    return;
  }
  image_database_initialized_ = true;
  if (IsInitialized()) {
    ProcessPendingLoads();
  }
}

void RemoteSuggestionsDatabase::OnImageDatabaseLoaded(
    SnippetImageCallback callback,
    bool success,
    std::unique_ptr<SnippetImageProto> entry) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase load failed.";
    OnDatabaseError();
    return;
  }

  if (!entry) {
    std::move(callback).Run(std::string());
    return;
  }

  std::unique_ptr<std::string> data(entry->release_data());
  std::move(callback).Run(std::move(*data));
}

void RemoteSuggestionsDatabase::OnImageDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "RemoteSuggestionsDatabase save failed.";
    OnDatabaseError();
  }
}

void RemoteSuggestionsDatabase::OnDatabaseError() {
  database_.reset();
  image_database_.reset();
  if (!error_callback_.is_null()) {
    error_callback_.Run();
  }
}

void RemoteSuggestionsDatabase::ProcessPendingLoads() {
  DCHECK(IsInitialized());

  for (auto& callback : pending_snippets_callbacks_) {
    LoadSnippetsImpl(std::move(callback));
  }
  pending_snippets_callbacks_.clear();

  for (auto& id_callback : pending_image_callbacks_) {
    LoadImageImpl(id_callback.first, std::move(id_callback.second));
  }
  pending_image_callbacks_.clear();
}

void RemoteSuggestionsDatabase::LoadSnippetsImpl(SnippetsCallback callback) {
  DCHECK(IsInitialized());
  database_->LoadEntries(
      base::BindOnce(&RemoteSuggestionsDatabase::OnDatabaseLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RemoteSuggestionsDatabase::SaveSnippetsImpl(
    std::unique_ptr<KeyEntryVector> entries_to_save) {
  DCHECK(IsInitialized());

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::BindOnce(&RemoteSuggestionsDatabase::OnDatabaseSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteSuggestionsDatabase::LoadImageImpl(const std::string& snippet_id,
                                              SnippetImageCallback callback) {
  DCHECK(IsInitialized());
  image_database_->GetEntry(
      snippet_id,
      base::BindOnce(&RemoteSuggestionsDatabase::OnImageDatabaseLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RemoteSuggestionsDatabase::DeleteUnreferencedImages(
    std::unique_ptr<std::set<std::string>> references,
    bool load_keys_success,
    std::unique_ptr<std::vector<std::string>> image_keys) {
  if (!load_keys_success) {
    DVLOG(1) << "RemoteSuggestionsDatabase garbage collection failed.";
    OnDatabaseError();
    return;
  }
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  for (const std::string& key : *image_keys) {
    if (references->count(key) == 0) {
      keys_to_remove->emplace_back(key);
    }
  }
  if (keys_to_remove->empty())
    return;
  DeleteImages(std::move(keys_to_remove));
}

}  // namespace ntp_snippets
