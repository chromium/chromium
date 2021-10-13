// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"

namespace autofill {

class ContentAutofillDriverFactoryTestApi {
 public:
  static std::unique_ptr<ContentAutofillDriverFactory> Create(
      content::WebContents* web_contents,
      AutofillClient* client,
      const std::string& app_locale,
      BrowserAutofillManager::AutofillDownloadManagerState
          enable_download_manager,
      AutofillManager::AutofillManagerFactoryCallback
          autofill_manager_factory_callback);

  explicit ContentAutofillDriverFactoryTestApi(
      ContentAutofillDriverFactory* factory);

  size_t num_drivers() const { return factory_->driver_map_.size(); }

  ContentAutofillDriver* GetDriver(content::RenderFrameHost* rfh);

  ContentAutofillRouter& router() { return factory_->router_; }

 private:
  ContentAutofillDriverFactory* factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
