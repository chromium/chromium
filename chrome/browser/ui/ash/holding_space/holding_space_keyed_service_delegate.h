// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observer.h"

class Profile;

namespace ash {

// Abstract class for a delegate of `HoldingSpaceKeyedService`. Multiple
// delegates will exist, each with an independent area of responsibility.
class HoldingSpaceKeyedServiceDelegate : public HoldingSpaceModelObserver {
 public:
  ~HoldingSpaceKeyedServiceDelegate() override;

  // Invoked by `HoldingSpaceKeyedService` to initialize the delegate.
  // Called immediately after the delegate's construction. Delegates accepting
  // callbacks from the service should *not* invoke callbacks during
  // construction but are free to do so during or anytime after initialization.
  virtual void Init() = 0;

  // Invoked by `HoldingSpaceKeyedService` to notify delegates when holding
  // space persistence has been restored.
  void NotifyPersistenceRestored();

 protected:
  HoldingSpaceKeyedServiceDelegate(Profile* profile, HoldingSpaceModel* model);

  // Returns the `profile_` associated with the `HoldingSpaceKeyedService`.
  Profile* profile() { return profile_; }

  // Returns the holding space model owned by `HoldingSpaceKeyedService`.
  HoldingSpaceModel* model() { return model_; }

  // Returns if persistence is being restored.
  bool is_restoring_persistence() const { return is_restoring_persistence_; }

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

  // Invoked when holding space persistence has been restored.
  virtual void OnPersistenceRestored();

  Profile* const profile_;
  HoldingSpaceModel* const model_;

  // If persistence is being restored.
  bool is_restoring_persistence_ = true;

  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver>
      holding_space_model_observer_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_
