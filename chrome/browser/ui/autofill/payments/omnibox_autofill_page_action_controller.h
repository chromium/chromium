// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_PAGE_ACTION_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class OmniboxAutofillPageActionController final
    : public page_actions::PageActionObserver {
 public:
  DECLARE_USER_DATA(OmniboxAutofillPageActionController);

  OmniboxAutofillPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~OmniboxAutofillPageActionController() override;

  OmniboxAutofillPageActionController(
      const OmniboxAutofillPageActionController&) = delete;
  OmniboxAutofillPageActionController& operator=(
      const OmniboxAutofillPageActionController&) = delete;

  static OmniboxAutofillPageActionController* From(tabs::TabInterface& tab);

  // page_actions::PageActionObserver:
  void OnPageActionChipShown(
      const page_actions::PageActionState& page_action) override;

  // Shows the expanded omnibox autofill page action chip with icon and label.
  void ShowExpandedChip(base::OnceClosure on_chip_shown);

  // Shows the collapsed omnibox autofill page action chip with icon only.
  void ShowCollapsedChip();

  // Hides the entire omnibox autofill page action chip.
  void HideChip();

 private:
  const raw_ref<tabs::TabInterface> tab_interface_;

  const raw_ref<page_actions::PageActionController> page_action_controller_;

  base::OnceClosure on_chip_shown_;

  ui::ScopedUnownedUserData<OmniboxAutofillPageActionController>
      scoped_unowned_user_data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_PAGE_ACTION_CONTROLLER_H_
