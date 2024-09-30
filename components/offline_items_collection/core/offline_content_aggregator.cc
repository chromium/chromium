// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"

namespace offline_items_collection {

namespace {

template <typename T, typename U>
bool MapContainsValue(const std::map<T, raw_ptr<U, CtnExperimental>>& map,
                      U* value) {
  for (const auto& it : map) {
    if (it.second == value)
      return true;
  }
  return false;
}

}  // namespace

OfflineContentAggregator::OfflineContentAggregator() {}

OfflineContentAggregator::~OfflineContentAggregator() = default;

std::string OfflineContentAggregator::CreateUniqueNameSpace(
    const std::string& prefix,
    bool is_off_the_record) {
  if (!is_off_the_record)
    return prefix;

  static int num_registrations = 0;
  return prefix + "_" + base::NumberToString(++num_registrations);
}

void OfflineContentAggregator::RegisterProvider(
    const std::string& name_space,
    OfflineContentProvider* provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Validate that this is the first OfflineContentProvider registered that is
  // associated with |name_space|.
  DCHECK(providers_.find(name_space) == providers_.end());

  // Only set up the connection to the provider if the provider isn't associated
  // with any other namespace.
  if (!MapContainsValue(providers_, provider))
    provider->AddObserver(this);

  providers_[name_space] = provider;
}

void OfflineContentAggregator::UnregisterProvider(
    const std::string& name_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto provider_it = providers_.find(name_space);
  CHECK(provider_it != providers_.end());

  OfflineContentProvider* provider = provider_it->second;
  providers_.erase(provider_it);
  pending_providers_.erase(provider);

  // Only clean up the connection to the provider if the provider isn't
  // associated with any other namespace.
  if (!MapContainsValue(providers_, provider)) {
    provider->RemoveObserver(this);
  }
}

void OfflineContentAggregator::OpenItem(const OpenParams& open_params,
                                        const ContentId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  it->second->OpenItem(open_params, id);
}

void OfflineContentAggregator::RemoveItem(const ContentId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  it->second->RemoveItem(id);
}

void OfflineContentAggregator::CancelDownload(const ContentId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  it->second->CancelDownload(id);
}

void OfflineContentAggregator::PauseDownload(const ContentId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  it->second->PauseDownload(id);
}

void OfflineContentAggregator::ResumeDownload(const ContentId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  it->second->ResumeDownload(id);
}

void OfflineContentAggregator::GetItemById(const ContentId& id,
                                           SingleItemCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);
  if (it == providers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  it->second->GetItemById(
      id, base::BindOnce(&OfflineContentAggregator::OnGetItemByIdDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OfflineContentAggregator::OnGetItemByIdDone(
    SingleItemCallback callback,
    const std::optional<OfflineItem>& item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(item);
}

void OfflineContentAggregator::GetAllItems(MultipleItemCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is already a call in progress, queue up the callback and wait for
  // the results.
  if (!multiple_item_get_callbacks_.empty()) {
    multiple_item_get_callbacks_.push_back(std::move(callback));
    return;
  }

  DCHECK(aggregated_items_.empty());
  for (auto provider_it : providers_) {
    auto* provider = provider_it.second.get();

    provider->GetAllItems(
        base::BindOnce(&OfflineContentAggregator::OnGetAllItemsDone,
                       weak_ptr_factory_.GetWeakPtr(), provider));
    pending_providers_.insert(provider);
  }

  if (pending_providers_.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), OfflineItemList()));
    return;
  }

  multiple_item_get_callbacks_.push_back(std::move(callback));
}

void OfflineContentAggregator::OnGetAllItemsDone(
    OfflineContentProvider* provider,
    const OfflineItemList& items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  aggregated_items_.insert(aggregated_items_.end(), items.begin(), items.end());
  pending_providers_.erase(provider);
  if (!pending_providers_.empty()) {
    return;
  }

  auto item_vec = std::move(aggregated_items_);
  auto callbacks = std::move(multiple_item_get_callbacks_);

  for (auto& callback : callbacks)
    std::move(callback).Run(item_vec);
}

void OfflineContentAggregator::GetVisualsForItem(const ContentId& id,
                                                 GetVisualsOptions options,
                                                 VisualsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = providers_.find(id.name_space);

  if (it == providers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

  it->second->GetVisualsForItem(id, options, std::move(callback));
}

void OfflineContentAggregator::GetShareInfoForItem(const ContentId& id,
                                                   ShareCallback callback) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

  it->second->GetShareInfoForItem(id, std::move(callback));
}

void OfflineContentAggregator::RenameItem(const ContentId& id,
                                          const std::string& name,
                                          RenameCallback callback) {
  auto it = providers_.find(id.name_space);
  if (it == providers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RenameResult::FAILURE_UNAVAILABLE));
    return;
  }
  it->second->RenameItem(id, name, std::move(callback));
}

void OfflineContentAggregator::OnItemsAdded(const OfflineItemList& items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyItemsAdded(items);
}

void OfflineContentAggregator::OnItemRemoved(const ContentId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pending_providers_.empty()) {
    auto item = base::ranges::find(aggregated_items_, id, &OfflineItem::id);
    if (item != aggregated_items_.end())
      aggregated_items_.erase(item);
  }
  NotifyItemRemoved(id);
}

void OfflineContentAggregator::OnItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_providers_.empty()) {
    for (auto& offline_item : aggregated_items_) {
      if (offline_item.id == item.id) {
        offline_item = item;
        break;
      }
    }
  }
  NotifyItemUpdated(item, update_delta);
}

void OfflineContentAggregator::OnContentProviderGoingDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Providers already call UnregisterProvider() manually for cleanup.
  // TODO(nicolaso): Find a less error-prone way to do this.
}

}  // namespace offline_items_collection
