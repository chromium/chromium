// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

// static
constexpr char HoldingSpacePersistenceDelegate::kPersistencePath[];

HoldingSpacePersistenceDelegate::HoldingSpacePersistenceDelegate(
    Profile* profile,
    HoldingSpaceModel* model,
    HoldingSpaceThumbnailLoader* thumbnail_loader,
    ItemRestoredCallback item_restored_callback,
    PersistenceRestoredCallback persistence_restored_callback)
    : HoldingSpaceKeyedServiceDelegate(profile, model),
      thumbnail_loader_(thumbnail_loader),
      item_restored_callback_(item_restored_callback),
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

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  if (is_restoring_persistence())
    return;

  // `kDownload` type holding space items have their own persistence mechanism.
  if (item->type() == HoldingSpaceItem::Type::kDownload)
    return;

  // Write the new |item| to persistent storage.
  ListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  update->Append(item->Serialize());
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  if (is_restoring_persistence())
    return;

  // `kDownload` type holding space items have their own persistence mechanism.
  if (item->type() == HoldingSpaceItem::Type::kDownload)
    return;

  // Remove the |item| from persistent storage.
  ListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  update->EraseListValueIf([&item](const base::Value& existing_item) {
    return HoldingSpaceItem::DeserializeId(
               base::Value::AsDictionaryValue(existing_item)) == item->id();
  });
}

void HoldingSpacePersistenceDelegate::RestoreModelFromPersistence() {
  DCHECK(model()->items().empty());

  const auto* persisted_holding_space_items =
      profile()->GetPrefs()->GetList(kPersistencePath);

  // If persistent storage is empty we can immediately notify the callback of
  // persistence restoration completion and quit early.
  if (persisted_holding_space_items->GetList().empty()) {
    std::move(persistence_restored_callback_).Run();
    return;
  }

  std::vector<HoldingSpaceItemPtr> holding_space_items;
  std::vector<base::FilePath> holding_space_file_paths;

  for (const auto& persisted_holding_space_item :
       persisted_holding_space_items->GetList()) {
    holding_space_items.push_back(HoldingSpaceItem::Deserialize(
        base::Value::AsDictionaryValue(persisted_holding_space_item),
        base::BindOnce(&holding_space_util::ResolveFileSystemUrl,
                       base::Unretained(profile())),
        base::BindOnce(&holding_space_util::ResolveImage,
                       base::Unretained(thumbnail_loader_))));
    holding_space_file_paths.push_back(holding_space_items.back()->file_path());
  }

  holding_space_util::PartitionFilePathsByExistence(
      profile(), std::move(holding_space_file_paths),
      base::BindOnce(&HoldingSpacePersistenceDelegate::RestoreModelByExistence,
                     weak_factory_.GetWeakPtr(),
                     std::move(holding_space_items)));
}

void HoldingSpacePersistenceDelegate::RestoreModelByExistence(
    std::vector<HoldingSpaceItemPtr> holding_space_items,
    std::vector<base::FilePath> existing_file_paths,
    std::vector<base::FilePath> non_existing_file_paths) {
  DCHECK(model()->items().empty());

  // Restore existing holding space items.
  for (auto& holding_space_item : holding_space_items) {
    if (base::Contains(existing_file_paths, holding_space_item->file_path()))
      item_restored_callback_.Run(std::move(holding_space_item));
  }

  // Clean up non-existing holding space items from persistence.
  if (!non_existing_file_paths.empty()) {
    ListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
    update->EraseListValueIf(
        [&non_existing_file_paths](const base::Value& persisted_item) {
          base::FilePath persisted_file_path =
              HoldingSpaceItem::DeserializeFilePath(
                  base::Value::AsDictionaryValue(persisted_item));
          return base::Contains(non_existing_file_paths, persisted_file_path);
        });
  }

  // Notify completion of persistence restoration.
  std::move(persistence_restored_callback_).Run();
}

}  // namespace ash
