// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace base {
class FilePath;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceThumbnailLoader;

using HoldingSpaceItemPtr = std::unique_ptr<HoldingSpaceItem>;

// A delegate of `HoldingSpaceKeyedService` tasked with persistence of holding
// space items. This includes the initial restoration of the model as well as
// incremental updates as items are added/removed at runtime.
class HoldingSpacePersistenceDelegate
    : public HoldingSpaceKeyedServiceDelegate {
 public:
  // TODO(crbug.com/1131266): Move to `ash::holding_space_prefs`.
  // Preference path at which holding space items are persisted.
  // NOTE: Any changes to persistence must be backwards compatible.
  static constexpr char kPersistencePath[] = "ash.holding_space.items";

  // Callback to invoke when the specified holding space item has been restored
  // from persistence.
  using ItemRestoredCallback =
      base::RepeatingCallback<void(HoldingSpaceItemPtr)>;

  // Callback to invoke when holding space persistence has been restored.
  using PersistenceRestoredCallback = base::OnceClosure;

  HoldingSpacePersistenceDelegate(
      Profile* profile,
      HoldingSpaceModel* model,
      HoldingSpaceThumbnailLoader* thumbnail_loader,
      ItemRestoredCallback item_restored_callback,
      PersistenceRestoredCallback persistence_restored_callback);
  HoldingSpacePersistenceDelegate(const HoldingSpacePersistenceDelegate&) =
      delete;
  HoldingSpacePersistenceDelegate& operator=(
      const HoldingSpacePersistenceDelegate&) = delete;
  ~HoldingSpacePersistenceDelegate() override;

  // Registers profile preferences for holding space.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // HoldingSpaceKeyedServiceDelegate:
  void Init() override;
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;

  // Restores the holding space model from persistent storage.
  void RestoreModelFromPersistence();
  void RestoreModelByValidity(
      std::vector<HoldingSpaceItemPtr> holding_space_items,
      std::vector<base::FilePath> valid_file_paths,
      std::vector<base::FilePath> invalid_file_paths);

  // Owned by `HoldingSpaceKeyedService`.
  HoldingSpaceThumbnailLoader* const thumbnail_loader_;

  // Callback to invoke when an item has been restored from persistence.
  ItemRestoredCallback item_restored_callback_;

  // Callback to invoke when holding space persistence has been restored.
  PersistenceRestoredCallback persistence_restored_callback_;

  base::WeakPtrFactory<HoldingSpacePersistenceDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_PERSISTENCE_DELEGATE_H_
