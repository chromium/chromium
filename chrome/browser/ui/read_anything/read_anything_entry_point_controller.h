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

  // Returns whether Reading Mode is currently showing. This handles both
  // Immersive and Side Panel reading mode.
  static bool IsUIShowing(BrowserWindowInterface* bwi);

  // Shows Reading Mode.
  static void ShowUI(BrowserWindowInterface* bwi,
                     ReadAnythingOpenTrigger open_trigger);

  // Shows or hides the omnibox entry point and the IPH for it.
  // show_promo_callback is called with the result of whether the IPH was shown.
  static void UpdatePageActionVisibility(
      bool should_show_page_action,
      BrowserWindowInterface* bwi,
      base::OnceCallback<void(user_education::FeaturePromoResult promo_result)>
          show_promo_callback = {});

  // Updates the number of times the omnibox entry point has been ignored by the
  // user.
  static void OnPageActionIgnored(BrowserWindowInterface* bwi);

  // Returns false if the reading mode suggestion should be hidden immediately
  // for the current page. This is separate from CheckIfShouldSuggestReadingMode
  // to allow callers to avoid running the asynchronous readability heuristic if
  // they just want to do a quick synchronous check if the suggestion should be
  // hidden.
  static bool CheckIfShouldSuggestReadingModeNaive(BrowserWindowInterface* bwi);

  // Checks whether to suggest reading mode to the user on the current page and
  // asynchronously returns the result via `result_callback`. This is
  // asynchronous because it runs a heuristic to determine if the page is a good
  // candidate for reading mode.
  static void CheckIfShouldSuggestReadingMode(
      BrowserWindowInterface* bwi,
      base::OnceCallback<void(bool)> result_callback);

 private:
  static void ToggleUI(BrowserWindowInterface* bwi,
                       ReadAnythingOpenTrigger open_trigger);
};

}  // namespace read_anything

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_
