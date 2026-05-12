// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class OmniboxAutofillPageActionController {
 public:
  DECLARE_USER_DATA(OmniboxAutofillPageActionController);

  OmniboxAutofillPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~OmniboxAutofillPageActionController();

  OmniboxAutofillPageActionController(
      const OmniboxAutofillPageActionController&) = delete;
  OmniboxAutofillPageActionController& operator=(
      const OmniboxAutofillPageActionController&) = delete;

  static OmniboxAutofillPageActionController* From(tabs::TabInterface& tab);

  // Shows the omnibox autofill page action icon.
  void Show();

  // Hides the omnibox autofill page action icon.
  void Hide();

 private:
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  ui::ScopedUnownedUserData<OmniboxAutofillPageActionController>
      scoped_unowned_user_data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_PAGE_ACTION_CONTROLLER_H_
