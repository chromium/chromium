// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/actions/actions.h"

namespace read_anything {

// Maintains and handles entry points for the ReadAnythingController.
class ReadAnythingEntryPointController {
 public:
  ReadAnythingEntryPointController(const ReadAnythingEntryPointController&) =
      delete;
  ReadAnythingEntryPointController& operator=(
      const ReadAnythingEntryPointController&) = delete;
  ~ReadAnythingEntryPointController();

  // Toggles Reading Mode on or off.
  static void InvokePageAction(BrowserWindowInterface* bwi,
                               const actions::ActionInvocationContext& context);

  // Shows Reading Mode.
  static void ShowUI(BrowserWindowInterface* bwi,
                     ReadAnythingOpenTrigger open_trigger);

  // Shows or hides the omnibox entry point and the IPH for it.
  // show_promo_callback is called with the result of whether the IPH was shown.
  // TODO(crbug.com/447418049): Ensure immersive reading mode shows and hides
  // the omnibox entry point too, and use the callback here, or refactor such
  // that immersive and side panel share the same logic.
  static void UpdatePageActionVisibility(
      bool should_show_page_action,
      BrowserWindowInterface* bwi,
      base::OnceCallback<void(user_education::FeaturePromoResult promo_result)>
          show_promo_callback = {});

 private:
  static void ToggleUI(BrowserWindowInterface* bwi,
                       ReadAnythingOpenTrigger open_trigger);
};

}  // namespace read_anything

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_
