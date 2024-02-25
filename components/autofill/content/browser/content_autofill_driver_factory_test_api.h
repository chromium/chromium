// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"

namespace autofill {

class ContentAutofillDriverFactoryTestApi {
 public:
  static std::unique_ptr<ContentAutofillDriverFactory> Create(
      content::WebContents* web_contents,
      ContentAutofillClient* client);

  explicit ContentAutofillDriverFactoryTestApi(
      ContentAutofillDriverFactory* factory);

  size_t num_drivers() const { return factory_->driver_map_.size(); }

  // Replaces the existing driver with `new_driver`. An existing driver must
  // exist. This does not invalidate references. More precisely:
  //
  //   std::unique_ptr<ContentAutofillDriver>& old_driver = ...;
  //   std::unique_ptr<ContentAutofillDriver> new_driver = ...;
  //   ContentAutofillDriver* new_driver_raw = new_driver.get();
  //   ExchangeDriver(rfh, std::move(new_driver));
  //   CHECK_EQ(old_driver.get(), new_driver_raw);
  std::unique_ptr<ContentAutofillDriver> ExchangeDriver(
      content::RenderFrameHost* rfh,
      std::unique_ptr<ContentAutofillDriver> new_driver);

  ContentAutofillDriver* GetDriver(content::RenderFrameHost* rfh);

  base::ObserverList<ContentAutofillDriverFactory::Observer>& observers() {
    return factory_->observers_;
  }

  // Like the normal AddObserver(), but enqueues `observer` at position `index`
  // in the list, so that `observer` is notified before production-code
  // observers.
  void AddObserverAtIndex(ContentAutofillDriverFactory::Observer* observer,
                          size_t index);

 private:
  const raw_ref<ContentAutofillDriverFactory> factory_;
};

inline ContentAutofillDriverFactoryTestApi test_api(
    ContentAutofillDriverFactory& factory) {
  return ContentAutofillDriverFactoryTestApi(&factory);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_FACTORY_TEST_API_H_
