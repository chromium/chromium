// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/model_impl.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/model_stats.h"

namespace download {

ModelImpl::ModelImpl(std::unique_ptr<Store> store)
    : client_(nullptr), store_(std::move(store)) {
  DCHECK(store_);
}

ModelImpl::~ModelImpl() = default;

void ModelImpl::Initialize(Client* client) {
  DCHECK(!client_);
  client_ = client;
  DCHECK(client_);

  DCHECK(!store_->IsInitialized());
  store_->Initialize(base::BindOnce(&ModelImpl::OnInitializedFinished,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ModelImpl::HardRecover() {
  entries_.clear();

  store_->HardRecover(base::BindOnce(&ModelImpl::OnHardRecoverFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ModelImpl::Add(const Entry& entry) {
  DCHECK(store_->IsInitialized());
  DCHECK(entries_.find(entry.guid) == entries_.end());

  entries_.emplace(entry.guid, std::make_unique<Entry>(entry));

  store_->Update(entry, base::BindOnce(&ModelImpl::OnAddFinished,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       entry.client, entry.guid));
}

void ModelImpl::Update(const Entry& entry) {
  DCHECK(store_->IsInitialized());
  DCHECK(entries_.find(entry.guid) != entries_.end());

  *entries_[entry.guid] = entry;

  store_->Update(entry, base::BindOnce(&ModelImpl::OnUpdateFinished,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       entry.client, entry.guid));
}

void ModelImpl::Remove(const std::string& guid) {
  DCHECK(store_->IsInitialized());

  const auto& it = entries_.find(guid);
  CHECK(it != entries_.end(), base::NotFatalUntil::M130);

  // Pull out a separate guid and a DownloadClient so that when we destroy the
  // entry we don't destroy the std::string that is backing the guid.
  std::string standalone_guid = guid;
  DownloadClient client = it->second->client;
  entries_.erase(it);
  store_->Remove(standalone_guid, base::BindOnce(&ModelImpl::OnRemoveFinished,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 client, standalone_guid));
}

Entry* ModelImpl::Get(const std::string& guid) {
  const auto& it = entries_.find(guid);
  return it == entries_.end() ? nullptr : it->second.get();
}

Model::EntryList ModelImpl::PeekEntries() {
  EntryList entries;
  for (const auto& it : entries_)
    entries.push_back(it.second.get());

  return entries;
}

void ModelImpl::OnInitializedFinished(
    bool success,
    std::unique_ptr<std::vector<Entry>> entries) {
  stats::LogModelOperationResult(stats::ModelAction::kInitialize, success);

  if (!success) {
    client_->OnModelReady(false);
    return;
  }

  std::map<Entry::State, uint32_t> entries_count;
  for (const auto& entry : *entries) {
    entries_count[entry.state]++;
    entries_.emplace(entry.guid, std::make_unique<Entry>(entry));
  }

  stats::LogEntries(entries_count);
  client_->OnModelReady(true);
}

void ModelImpl::OnHardRecoverFinished(bool success) {
  client_->OnModelHardRecoverComplete(success);
}

void ModelImpl::OnAddFinished(DownloadClient client,
                              const std::string& guid,
                              bool success) {
  stats::LogModelOperationResult(stats::ModelAction::kAdd, success);

  // Don't notify the Client if the entry was already removed.
  auto it = entries_.find(guid);
  if (it == entries_.end())
    return;

  // Remove the entry from the map if the add failed.
  if (!success) {
    entries_.erase(it);
  }

  client_->OnItemAdded(success, client, guid);
}

void ModelImpl::OnUpdateFinished(DownloadClient client,
                                 const std::string& guid,
                                 bool success) {
  stats::LogModelOperationResult(stats::ModelAction::kUpdate, success);

  // Don't notify the Client if the entry was already removed.
  if (entries_.find(guid) == entries_.end())
    return;

  client_->OnItemUpdated(success, client, guid);
}

void ModelImpl::OnRemoveFinished(DownloadClient client,
                                 const std::string& guid,
                                 bool success) {
  stats::LogModelOperationResult(stats::ModelAction::kRemove, success);

  DCHECK(entries_.find(guid) == entries_.end());
  client_->OnItemRemoved(success, client, guid);
}

size_t ModelImpl::EstimateMemoryUsage() const {
  // Only track in-memory cache size.
  return base::trace_event::EstimateMemoryUsage(entries_);
}

}  // namespace download
