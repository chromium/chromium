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
#include "components/autofill/core/browser/test_autofill_client.h"

namespace autofill {

class ContentAutofillDriverFactoryTestApi {
 public:
  // Creates a factory of ContentAutofillDrivers whose managers are
  // TestBrowserAutofillManager.
  static std::unique_ptr<ContentAutofillDriverFactory> Create(
      content::WebContents* web_contents,
      TestAutofillClient* client);

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

  base::ObserverList<ContentAutofillDriverFactory::Observer>& observers() {
    return factory_->observers_;
  }

  // Like the normal AddObserver(), but enqueues `observer` at position `index`
  // in the list, so that `observer` is notified before production-code
  // observers.
  void AddObserverAtIndex(ContentAutofillDriverFactory::Observer* observer,
                          size_t index);

  void set_client(AutofillClient* client) { factory_->client_ = client; }

  ContentAutofillRouter& router() { return factory_->router_; }

 private:
  raw_ptr<ContentAutofillDriverFactory> factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
