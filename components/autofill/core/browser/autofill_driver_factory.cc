// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_factory.h"

#include "base/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"

namespace autofill {

AutofillDriverFactory::AutofillDriverFactory(AutofillClient* client)
    : client_(client) {}

AutofillDriverFactory::~AutofillDriverFactory() {}

AutofillDriver* AutofillDriverFactory::DriverForKey(void* key) {
  auto mapping = driver_map_.find(key);
  return mapping == driver_map_.end() ? nullptr : mapping->second.get();
}

void AutofillDriverFactory::NavigationFinished(HideUi hide_ui) {
  if (hide_ui)
    client_->HideAutofillPopup(PopupHidingReason::kNavigation);
}

void AutofillDriverFactory::TabHidden() {
  client_->HideAutofillPopup(PopupHidingReason::kTabGone);
}

void AutofillDriverFactory::AddForKey(
    void* key,
    const base::RepeatingCallback<std::unique_ptr<AutofillDriver>()>&
        factory_method) {
  auto insertion_result = driver_map_.insert(std::make_pair(key, nullptr));
  // This can be called twice for the key representing the main frame.
  if (insertion_result.second) {
    insertion_result.first->second = factory_method.Run();
  }
}

void AutofillDriverFactory::DeleteForKey(void* key) {
  driver_map_.erase(key);
}

}  // namespace autofill
