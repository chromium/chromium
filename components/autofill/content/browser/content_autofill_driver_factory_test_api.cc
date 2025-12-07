// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"

namespace autofill {

// static
std::unique_ptr<ContentAutofillDriverFactory>
ContentAutofillDriverFactoryTestApi::Create(content::WebContents* web_contents,
                                            ContentAutofillClient* client) {
  return std::make_unique<ContentAutofillDriverFactory>(web_contents, client);
}

ContentAutofillDriverFactoryTestApi::ContentAutofillDriverFactoryTestApi(
    ContentAutofillDriverFactory* factory)
    : AutofillDriverFactoryTestApi(factory) {}

std::unique_ptr<ContentAutofillDriver>
ContentAutofillDriverFactoryTestApi::ExchangeDriver(
    content::RenderFrameHost* rfh,
    std::unique_ptr<ContentAutofillDriver> new_driver) {
  auto it = factory().driver_map_.find(rfh);
  CHECK(it != factory().driver_map_.end());
  std::unique_ptr<ContentAutofillDriver> old_driver =
      std::exchange(it->second, std::move(new_driver));

  // Make sure that `old_driver` is in `LifecycleState::kPendingDeletion` to
  // avoid hitting a CHECK in ~AutofillDriver().
  // To prevent firing events, we create a fresh AutofillDriverFactory that has
  // no observers.
  class : public AutofillDriverFactory {
   public:
    using AutofillDriverFactory::SetLifecycleStateAndNotifyObservers;
    std::vector<AutofillDriver*> GetExistingDrivers() override { return {}; }
  } helper;
  helper.SetLifecycleStateAndNotifyObservers(
      *old_driver, AutofillDriver::LifecycleState::kPendingDeletion);
  return old_driver;
}

ContentAutofillDriver* ContentAutofillDriverFactoryTestApi::DriverForFrame(
    content::RenderFrameHost* rfh) {
  return factory().DriverForFrame(rfh);
}

ContentAutofillDriver* ContentAutofillDriverFactoryTestApi::GetDriver(
    content::RenderFrameHost* rfh) {
  auto it = factory().driver_map_.find(rfh);
  return it != factory().driver_map_.end() ? it->second.get() : nullptr;
}

void ContentAutofillDriverFactoryTestApi::Reset(ContentAutofillDriver& driver) {
  using enum AutofillDriver::LifecycleState;
  AutofillDriver::LifecycleState original_state = driver.GetLifecycleState();
  CHECK(original_state == kActive || original_state == kInactive);
  factory().SetLifecycleStateAndNotifyObservers(driver, kPendingReset);
  factory().SetLifecycleStateAndNotifyObservers(driver, original_state);
}

void ContentAutofillDriverFactoryTestApi::SetLifecycleStateAndNotifyObservers(
    ContentAutofillDriver& driver,
    AutofillDriver::LifecycleState new_state) {
  factory().SetLifecycleStateAndNotifyObservers(driver, new_state);
}

}  // namespace autofill
