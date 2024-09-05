// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"

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
  using enum ContentAutofillDriver::LifecycleState;
  auto it = factory().driver_map_.find(rfh);
  CHECK(it != factory().driver_map_.end());
  std::unique_ptr<ContentAutofillDriver> old_driver =
      std::exchange(it->second, std::move(new_driver));
  test_api(*old_driver).SetLifecycleState(kPendingDeletion);
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

}  // namespace autofill
