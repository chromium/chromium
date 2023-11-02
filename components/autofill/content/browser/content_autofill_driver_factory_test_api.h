// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"

namespace autofill {

class ContentAutofillDriverFactoryTestApi {
 public:
  static std::unique_ptr<ContentAutofillDriverFactory> Create(
      content::WebContents* web_contents,
      AutofillClient* client,
      ContentAutofillDriverFactory::DriverInitCallback driver_init_hook);

  explicit ContentAutofillDriverFactoryTestApi(
      ContentAutofillDriverFactory* factory);

  size_t num_drivers() const { return factory_->driver_map_.size(); }

  void SetDriver(content::RenderFrameHost* rfh,
                 std::unique_ptr<ContentAutofillDriver> driver);
  ContentAutofillDriver* GetDriver(content::RenderFrameHost* rfh);

  ContentAutofillRouter& router() { return factory_->router_; }

 private:
  raw_ptr<ContentAutofillDriverFactory> factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
