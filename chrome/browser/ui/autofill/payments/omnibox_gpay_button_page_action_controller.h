// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_GPAY_BUTTON_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_GPAY_BUTTON_PAGE_ACTION_CONTROLLER_H_

namespace autofill {

class OmniboxGPayButtonPageActionController {
 public:
  OmniboxGPayButtonPageActionController();
  ~OmniboxGPayButtonPageActionController();
  OmniboxGPayButtonPageActionController(
      const OmniboxGPayButtonPageActionController&) = delete;
  OmniboxGPayButtonPageActionController& operator=(
      const OmniboxGPayButtonPageActionController&) = delete;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_GPAY_BUTTON_PAGE_ACTION_CONTROLLER_H_
