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
    : factory_(*factory) {}

std::unique_ptr<ContentAutofillDriver>
ContentAutofillDriverFactoryTestApi::ExchangeDriver(
    content::RenderFrameHost* rfh,
    std::unique_ptr<ContentAutofillDriver> new_driver) {
  auto it = factory_->driver_map_.find(rfh);
  CHECK(it != factory_->driver_map_.end());
  return std::exchange(it->second, std::move(new_driver));
}

ContentAutofillDriver* ContentAutofillDriverFactoryTestApi::GetDriver(
    content::RenderFrameHost* rfh) {
  auto it = factory_->driver_map_.find(rfh);
  return it != factory_->driver_map_.end() ? it->second.get() : nullptr;
}

void ContentAutofillDriverFactoryTestApi::AddObserverAtIndex(
    ContentAutofillDriverFactory::Observer* new_observer,
    size_t index) {
  std::vector<ContentAutofillDriverFactory::Observer*> observers;
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
