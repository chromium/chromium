// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class HoldingSpaceItem;
class ThumbnailLoader;

using HoldingSpaceItemPtr = std::unique_ptr<HoldingSpaceItem>;

// A delegate of `HoldingSpaceKeyedService` tasked with persistence of holding
// space items. This includes the initial restoration of the model as well as
// incremental updates as items are added/removed at runtime.
class HoldingSpacePersistenceDelegate
    : public HoldingSpaceKeyedServiceDelegate {
 public:
  // TODO(crbug.com/40150129): Move to `ash::holding_space_prefs`.
  // Preference path at which holding space items are persisted.
  // NOTE: Any changes to persistence must be backwards compatible.
  static constexpr char kPersistencePath[] = "ash.holding_space.items";

  // Callback to invoke when holding space persistence has been restored to
  // add the restored items to the holding space model.
  using PersistenceRestoredCallback =
      base::OnceCallback<void(std::vector<HoldingSpaceItemPtr>)>;

  HoldingSpacePersistenceDelegate(
      HoldingSpaceKeyedService* service,
      HoldingSpaceModel* model,
      ThumbnailLoader* thumbnail_loader,
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
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override;

  // Restores the holding space model from persistent storage.
  void RestoreModelFromPersistence();

  // Removes items from persistent storage that should not be restored to the
  // in-memory holding space model.
  void MaybeRemoveItemsFromPersistence();

  // Owned by `HoldingSpaceKeyedService`.
  const raw_ptr<ThumbnailLoader> thumbnail_loader_;

  // Callback to invoke when holding space persistence has been restored.
  PersistenceRestoredCallback persistence_restored_callback_;

  base::WeakPtrFactory<HoldingSpacePersistenceDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_PERSISTENCE_DELEGATE_H_
