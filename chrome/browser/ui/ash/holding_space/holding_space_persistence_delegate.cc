// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

// Returns whether the item should be ignored by the holding space model. This
// returns true if the item is not supported in the current context, but may
// be otherwise supported. For example, returns true for ARC file system
// backed items in a secondary user profile.
bool ShouldIgnoreItem(Profile* profile, const HoldingSpaceItem* item) {
  return file_manager::util::GetAndroidFilesPath().IsParent(
             item->file().file_path) &&
         !ProfileHelper::IsPrimaryProfile(profile);
}

}  // namespace

// static
constexpr char HoldingSpacePersistenceDelegate::kPersistencePath[];

HoldingSpacePersistenceDelegate::HoldingSpacePersistenceDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model,
    ThumbnailLoader* thumbnail_loader,
    PersistenceRestoredCallback persistence_restored_callback)
    : HoldingSpaceKeyedServiceDelegate(service, model),
      thumbnail_loader_(thumbnail_loader),
      persistence_restored_callback_(std::move(persistence_restored_callback)) {
}

HoldingSpacePersistenceDelegate::~HoldingSpacePersistenceDelegate() = default;

// static
void HoldingSpacePersistenceDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPersistencePath);
}

void HoldingSpacePersistenceDelegate::Init() {
  // We expect that the associated profile is already ready when we are being
  // initialized. That being the case, we can immediately proceed to restore
  // the holding space model from persistence storage.
  RestoreModelFromPersistence();
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (is_restoring_persistence())
    return;

  // Write the new finalized `items` to persistent storage.
  ScopedListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  for (const HoldingSpaceItem* item : items) {
    if (item->progress().IsComplete())
      update->Append(item->Serialize());
  }
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (is_restoring_persistence())
    return;

  // Remove the `items` from persistent storage.
  ScopedListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  update->EraseIf([&items](const base::Value& persisted_item) {
    const std::string& persisted_item_id =
        HoldingSpaceItem::DeserializeId(persisted_item.GetDict());
    return base::Contains(items, persisted_item_id, &HoldingSpaceItem::id);
  });
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item,
    const HoldingSpaceItemUpdatedFields& updated_fields) {
  if (is_restoring_persistence())
    return;

  // Only finalized items are persisted.
  if (!item->progress().IsComplete())
    return;

  // Attempt to find the finalized `item` in persistent storage.
  ScopedListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  base::Value::List& list = update.Get();
  auto item_it = base::ranges::find(
      list, item->id(), [](const base::Value& persisted_item) {
        return HoldingSpaceItem::DeserializeId(persisted_item.GetDict());
      });

  // If the finalized `item` already exists in persistent storage, update it.
  if (item_it != list.end()) {
    *item_it = base::Value(item->Serialize());
    return;
  }

  // If the finalized `item` did not previously exist in persistent storage,
  // insert it at the appropriate index.
  item_it = list.begin();
  for (const auto& candidate_item : model()->items()) {
    if (candidate_item.get() == item) {
      list.Insert(item_it, base::Value(item->Serialize()));
      return;
    }
    if (candidate_item->progress().IsComplete())
      ++item_it;
  }

  // The finalized `item` should exist in the model and be handled above.
  NOTREACHED_IN_MIGRATION();
}

void HoldingSpacePersistenceDelegate::RestoreModelFromPersistence() {
  DCHECK(model()->items().empty());

  // Remove items from persistent storage that should not be restored to the
  // in-memory holding space model.
  MaybeRemoveItemsFromPersistence();

  const base::Value::List& persisted_holding_space_items =
      profile()->GetPrefs()->GetList(kPersistencePath);

  // If persistent storage is empty we can immediately notify the callback of
  // persistence restoration completion and quit early.
  std::vector<std::unique_ptr<HoldingSpaceItem>> restored_items;
  if (persisted_holding_space_items.empty()) {
    std::move(persistence_restored_callback_).Run(std::move(restored_items));
    return;
  }

  for (const auto& persisted_holding_space_item :
       persisted_holding_space_items) {
    std::unique_ptr<HoldingSpaceItem> holding_space_item =
        HoldingSpaceItem::Deserialize(
            persisted_holding_space_item.GetDict(),
            base::BindOnce(&holding_space_util::ResolveImage,
                           base::Unretained(thumbnail_loader_)));

    if (!ShouldIgnoreItem(profile(), holding_space_item.get())) {
      restored_items.push_back(std::move(holding_space_item));
    }
  }

  // Notify completion of persistence restoration.
  std::move(persistence_restored_callback_).Run(std::move(restored_items));
}

void HoldingSpacePersistenceDelegate::MaybeRemoveItemsFromPersistence() {
  CHECK(is_restoring_persistence());

  const auto known_types = holding_space_util::GetAllItemTypes();

  const bool remove_suggestion_items =
      !features::IsHoldingSpaceSuggestionsEnabled();

  ScopedListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  update->EraseIf([&](const base::Value& persisted_item) {
    auto type = HoldingSpaceItem::DeserializeType(persisted_item.GetDict());

    // Remove items associated with unknown `type`s.
    if (!base::Contains(known_types, type)) {
      return true;
    }

    // Remove items associated with disabled features.
    if (remove_suggestion_items && HoldingSpaceItem::IsSuggestionType(type)) {
      return true;
    }

    return false;
  });
}

}  // namespace ash
