// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class LocationBarView;
class OmniboxController;
class Profile;

namespace omnibox {

// Controller for the AI mode page action icon. This class is responsible for
// deciding whether the AI mode icon should be shown in the omnibox.
class AiModePageActionController : public OmniboxEditModel::Observer {
 public:
  DECLARE_USER_DATA(AiModePageActionController);
  AiModePageActionController(BrowserWindowInterface& bwi,
                             Profile& profile,
                             LocationBarView& location_bar_view);

  ~AiModePageActionController() override;
  AiModePageActionController(const AiModePageActionController&) = delete;
  AiModePageActionController& operator=(const AiModePageActionController&) =
      delete;

  // OmniboxEditModel::Observer:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection new_selection) override {}
  void OnMatchIconUpdated(size_t index) override {}
  void OnContentsChanged() override;
  void OnKeywordStateChanged(bool is_keyword_selected) override {}
  void OnCharTyped(base::TimeTicks timestamp) override {}

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
  // Callback used in `UpdatePageAction()` to asynchronously fetch the favicon.
  void OnFaviconFetched(const gfx::Image& favicon);

  const raw_ref<BrowserWindowInterface> bwi_;
  const raw_ref<Profile> profile_;
  const raw_ref<LocationBarView> location_bar_view_;

  ui::ScopedUnownedUserData<AiModePageActionController> scoped_data_;

  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      observation_{this};

  // Used for `OnFaviconFetched()` callback.
  base::WeakPtrFactory<AiModePageActionController> weak_factory_{this};
};

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_
