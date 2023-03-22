// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_DRIVER_H_

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// A variant of TestAutofillDriver that can be associated with a
// content::WebContents.
//
// Consider using TestAutofillDriverInjector to inject the driver correctly,
// especially in browser tests.
class TestContentAutofillDriver
    : public TestAutofillDriverTemplate<ContentAutofillDriver> {
 public:
  using TestAutofillDriverTemplate<
      ContentAutofillDriver>::TestAutofillDriverTemplate;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_DRIVER_H_
