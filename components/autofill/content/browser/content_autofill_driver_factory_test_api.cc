// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"

#include "base/functional/bind.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"

namespace autofill {

// static
std::unique_ptr<ContentAutofillDriverFactory>
ContentAutofillDriverFactoryTestApi::Create(content::WebContents* web_contents,
                                            TestAutofillClient* client) {
  return Create(
      web_contents, client,
      base::BindRepeating(
          [](TestAutofillClient* client, ContentAutofillDriver* driver) {
            driver->set_autofill_manager(
                std::make_unique<TestBrowserAutofillManager>(driver, client));
          },
          client));
}

// static
std::unique_ptr<ContentAutofillDriverFactory>
ContentAutofillDriverFactoryTestApi::Create(
    content::WebContents* web_contents,
    AutofillClient* client,
    ContentAutofillDriverFactory::DriverInitCallback driver_init_hook) {
  return base::WrapUnique(
      new ContentAutofillDriverFactory(web_contents, client, driver_init_hook));
}

ContentAutofillDriverFactoryTestApi::ContentAutofillDriverFactoryTestApi(
    ContentAutofillDriverFactory* factory)
    : factory_(*factory) {}

void ContentAutofillDriverFactoryTestApi::SetDriver(
    content::RenderFrameHost* rfh,
    std::unique_ptr<ContentAutofillDriver> driver) {
  factory_->driver_map_[rfh] = std::move(driver);
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
