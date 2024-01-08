// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

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
  virtual void Init() {}

  // Invoked by `HoldingSpaceKeyedService` to notify delegates when holding
  // space persistence has been restored.
  void NotifyPersistenceRestored();

  // Returns if persistence is being restored.
  bool is_restoring_persistence() const { return is_restoring_persistence_; }

 protected:
  HoldingSpaceKeyedServiceDelegate(HoldingSpaceKeyedService* service,
                                   HoldingSpaceModel* model);

  // Returns the `profile_` associated with the `service_`.
  Profile* profile() { return service_->profile(); }

  // Returns the `service` which owns this delegate.
  HoldingSpaceKeyedService* service() { return service_; }

  // Returns the holding space model owned by `service_`.
  HoldingSpaceModel* model() { return model_; }

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override;

  // Invoked when holding space persistence has been restored.
  virtual void OnPersistenceRestored();

  const raw_ptr<HoldingSpaceKeyedService> service_;
  const raw_ptr<HoldingSpaceModel> model_;

  // If persistence is being restored.
  bool is_restoring_persistence_ = true;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      holding_space_model_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_
