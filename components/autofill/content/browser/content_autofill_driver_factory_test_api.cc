// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"

namespace autofill {

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
    : factory_(factory) {}

ContentAutofillDriver* ContentAutofillDriverFactoryTestApi::GetDriver(
    content::RenderFrameHost* rfh) {
  auto it = factory_->driver_map_.find(rfh);
  return it != factory_->driver_map_.end() ? it->second.get() : nullptr;
}

}  // namespace autofill
