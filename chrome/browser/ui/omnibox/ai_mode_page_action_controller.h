// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class LocationBarView;
class OmniboxController;
class Profile;

namespace omnibox {

// Controller for the AI mode page action icon. This class is responsible for
// deciding whether the AI mode icon should be shown in the omnibox.
class AiModePageActionController {
 public:
  DECLARE_USER_DATA(AiModePageActionController);
  AiModePageActionController(BrowserWindowInterface& bwi,
                             Profile& profile,
                             LocationBarView& location_bar_view);

  ~AiModePageActionController();
  AiModePageActionController(const AiModePageActionController&) = delete;
  AiModePageActionController& operator=(const AiModePageActionController&) =
      delete;

  // Determines whether the AI mode page action should be shown and updates
  // its visibility.
  void UpdatePageAction();

  // Unowned user data.
  static AiModePageActionController* From(BrowserWindowInterface* bwi);

  // Navigates current tab to AI mode.
  static void OpenAiMode(OmniboxController& omnibox_controller,
                         bool via_keyboard);

  // Notifies the OmniboxTriggeredFeatureService that the AI mode entrypoint has
  // been triggered.
  static void NotifyOmniboxTriggeredFeatureService(
      const OmniboxController& omnibox_controller);

  // Evaluates whether AI mode page action should be shown.
  static bool ShouldShowPageAction(Profile* profile,
                                   LocationBarView& location_bar_view);

 private:
  const raw_ref<BrowserWindowInterface> bwi_;
  const raw_ref<Profile> profile_;
  const raw_ref<LocationBarView> location_bar_view_;

  ui::ScopedUnownedUserData<AiModePageActionController> scoped_data_;
};

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_
