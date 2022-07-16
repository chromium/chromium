// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"

namespace autofill {

// static
std::unique_ptr<ContentAutofillDriverFactory>
ContentAutofillDriverFactoryTestApi::Create(
    content::WebContents* web_contents,
    AutofillClient* client,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback) {
  return base::WrapUnique(new ContentAutofillDriverFactory(
      web_contents, client, app_locale, enable_download_manager,
      autofill_manager_factory_callback));
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
