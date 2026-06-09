// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_AI_MODE_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class LocationBarView;
class OmniboxController;
class Profile;
class SkBitmap;

namespace gfx {
class Image;
}

namespace ui {
class ImageModel;
}

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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(AiModePageActionIconSource)
  enum class IconSource {
    // The page action was not shown.
    kInvisible = 0,
    // A built-in vector icon was used.
    kVectorIcon = 1,
    // The icon was loaded from omnibox's in-memory `FaviconCache`.
    kMemoryFaviconCache = 2,
    // The icon was loaded from the on-disk DB cache via `FaviconService`.
    kDiskDbFaviconCache = 3,
    // The icon was loaded from a network request via `BitmapFetcherService`.
    kNetworkFetch = 4,
    // The icon failed to load.
    kFailedIcon = 5,
    kMaxValue = kFailedIcon,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AiModePageActionIconSource)

  // Helper for `UpdatePageAction()`. Updates visibility and override image:
  // - If DSE is google, will use built-in image.
  // - If DSE is 3p, will check the in-memory favicon cache.
  // - If icon not found in the in-memory favicon cache, will check the on-disk
  //   favicon DB.
  // - If icon not found in the on-disk favicon DB, then will make a network
  //   request to fetch the icon.
  void SetPageActionVisibility(bool is_visible);

  // Helper for `SetPageActionVisibility()` to update visibility. `source` used
  // for logging.
  void Hide(IconSource source);

  // Helper for `SetPageActionVisibility()` to update the image and visibility.
  // `source` used for logging.
  void ShowAndOverrideImage(const ui::ImageModel& image, IconSource source);

  // Helpers used in `SetPageActionVisibility()` to asynchronously fetch the
  // favicon.
  void OnFaviconFetchedLocally(const GURL& favicon_url,
                               const gfx::Image& favicon);
  void FetchFaviconFromNetwork(const GURL& favicon_url);
  void OnFaviconFetchedFromNetwork(SkBitmap bitmap);

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
