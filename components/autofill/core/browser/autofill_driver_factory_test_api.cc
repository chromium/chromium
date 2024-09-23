// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_factory_test_api.h"

namespace autofill {

AutofillDriverFactoryTestApi::AutofillDriverFactoryTestApi(
    AutofillDriverFactory* factory)
    : factory_(*factory) {}

void AutofillDriverFactoryTestApi::AddObserverAtIndex(
    AutofillDriverFactory::Observer* new_observer,
    size_t index) {
  std::vector<AutofillDriverFactory::Observer*> observers;
  auto it = factory_->observers_.begin();
  for (; it != factory_->observers_.end() && index-- > 0; ++it) {
    observers.push_back(&*it);
  }
  observers.push_back(new_observer);
  for (; it != factory_->observers_.end(); ++it) {
    observers.push_back(&*it);
  }
  factory_->observers_.Clear();
  for (auto* observer : observers) {
    factory_->observers_.AddObserver(observer);
  }
}

}  // namespace autofill
