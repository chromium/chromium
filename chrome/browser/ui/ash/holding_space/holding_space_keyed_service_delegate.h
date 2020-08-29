// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observer.h"

namespace ash {

// Abstract class for a delegate of `HoldingSpaceKeyedService`. Multiple
// delegates will exist, each with an independent area of responsibility.
class HoldingSpaceKeyedServiceDelegate : public HoldingSpaceModelObserver {
 public:
  ~HoldingSpaceKeyedServiceDelegate() override;

 protected:
  explicit HoldingSpaceKeyedServiceDelegate(HoldingSpaceModel* model);

  // Returns the holding space model owned by `HoldingSpaceKeyedService`.
  const HoldingSpaceModel* model() const { return model_; }

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;

  // Owned by `HoldingSpaceKeyedService`.
  const HoldingSpaceModel* const model_;

  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver>
      holding_space_model_observer_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_DELEGATE_H_
