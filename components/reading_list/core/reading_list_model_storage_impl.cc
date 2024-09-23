// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model_storage_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/clock.h"
#include "base/trace_event/trace_event.h"
#include "components/reading_list/core/proto/reading_list.pb.h"
#include "components/sync/model/metadata_batch.h"
#include "url/gurl.h"

ReadingListModelStorageImpl::ScopedBatchUpdate::ScopedBatchUpdate(
    ReadingListModelStorageImpl* store)
    : store_(store) {
  store_->BeginTransaction();
}

ReadingListModelStorageImpl::ScopedBatchUpdate::~ScopedBatchUpdate() {
  store_->CommitTransaction();
}

void ReadingListModelStorageImpl::ScopedBatchUpdate::SaveEntry(
    const ReadingListEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_->batch_);

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry =
      entry.AsReadingListLocal(store_->clock_->Now());

  store_->batch_->WriteData(entry.URL().spec(), pb_entry->SerializeAsString());
}

void ReadingListModelStorageImpl::ScopedBatchUpdate::RemoveEntry(
    const GURL& entry_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_->batch_);
  store_->batch_->DeleteData(entry_url.spec());
}

ReadingListModelStorageImpl::ReadingListModelStorageImpl(
    syncer::OnceDataTypeStoreFactory create_store_callback)
    : create_store_callback_(std::move(create_store_callback)) {}

ReadingListModelStorageImpl::~ReadingListModelStorageImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0, pending_transaction_count_);
}

void ReadingListModelStorageImpl::Load(base::Clock* clock,
                                       LoadCallback load_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clock_ = clock;
  load_callback_ = std::move(load_cb);
  std::move(create_store_callback_)
      .Run(syncer::READING_LIST,
           base::BindOnce(&ReadingListModelStorageImpl::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate>
ReadingListModelStorageImpl::EnsureBatchCreated() {
  DCHECK(loaded_);
  return std::make_unique<ScopedBatchUpdate>(this);
}

void ReadingListModelStorageImpl::DeleteAllEntriesAndSyncMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_->DeleteAllDataAndMetadata(
      base::BindOnce(&ReadingListModelStorageImpl::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

syncer::MetadataChangeList*
ReadingListModelStorageImpl::ScopedBatchUpdate::GetSyncMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_->batch_);
  return store_->batch_->GetMetadataChangeList();
}

void ReadingListModelStorageImpl::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_transaction_count_++;
  if (pending_transaction_count_ == 1) {
    batch_ = store_->CreateWriteBatch();
  }
  DCHECK(batch_);
}

void ReadingListModelStorageImpl::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_transaction_count_--;
  if (pending_transaction_count_ == 0) {
    store_->CommitWriteBatch(
        std::move(batch_),
        base::BindOnce(&ReadingListModelStorageImpl::OnDatabaseSave,
                       weak_ptr_factory_.GetWeakPtr()));
    batch_.reset();
  }
}

void ReadingListModelStorageImpl::OnDatabaseLoad(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    std::move(load_callback_).Run(base::unexpected(error->message()));
    return;
  }

  ReadingListEntries loaded_entries;

  for (const syncer::DataTypeStore::Record& r : *entries) {
    reading_list::ReadingListLocal proto;
    if (!proto.ParseFromString(r.value)) {
      continue;
      // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
      // failure.
    }

    scoped_refptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListLocal(proto, clock_->Now()));
    if (!entry) {
      continue;
    }

    const GURL& url = entry->URL();
    DCHECK(!loaded_entries.count(url));
    loaded_entries.emplace(url, std::move(entry));
  }

  store_->ReadAllMetadata(base::BindOnce(
      &ReadingListModelStorageImpl::OnReadAllMetadata,
      weak_ptr_factory_.GetWeakPtr(), std::move(loaded_entries)));
}

void ReadingListModelStorageImpl::OnReadAllMetadata(
    ReadingListEntries loaded_entries,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "ReadingListModelStorageImpl::OnReadAllMetadata");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    std::move(load_callback_).Run(base::unexpected(error->message()));
  } else {
    loaded_ = true;
    std::move(load_callback_)
        .Run(std::make_pair(std::move(loaded_entries),
                            std::move(metadata_batch)));
  }
}

void ReadingListModelStorageImpl::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  return;
}

void ReadingListModelStorageImpl::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    std::move(load_callback_).Run(base::unexpected(error->message()));
    return;
  }
  store_ = std::move(store);
  store_->ReadAllData(
      base::BindOnce(&ReadingListModelStorageImpl::OnDatabaseLoad,
                     weak_ptr_factory_.GetWeakPtr()));
  return;
}
