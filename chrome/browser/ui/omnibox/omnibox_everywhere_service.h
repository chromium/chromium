// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class Profile;
class ScopedProfileKeepAlive;
class SkBitmap;

namespace user_education {
class FeaturePromoController;
}

namespace content {
class NavigationHandle;
}

namespace omnibox_everywhere {
class OmniboxEverywhereController;
class OmniboxEverywhereUIManager;
class OmniboxEverywhereFeaturePromoController;
}

class OmniboxEverywhereService : public KeyedService {
 public:
  struct RegionCaptureSource {
    enum class Type { kAllDisplays, kSpecificDisplay };
    Type type = Type::kAllDisplays;
    std::optional<int64_t> display_id;

    static RegionCaptureSource AllDisplays() {
      return {.type = Type::kAllDisplays};
    }
    static RegionCaptureSource ForDisplay(int64_t id) {
      return {.type = Type::kSpecificDisplay, .display_id = id};
    }
  };

  explicit OmniboxEverywhereService(Profile* profile);
  OmniboxEverywhereService(const OmniboxEverywhereService&) = delete;
  OmniboxEverywhereService& operator=(const OmniboxEverywhereService&) = delete;
  ~OmniboxEverywhereService() override;

  user_education::FeaturePromoController* feature_promo_controller();
  const user_education::FeaturePromoController* feature_promo_controller()
      const;

  virtual void HidePopup();
  virtual bool IsPopupVisible() const;
  virtual bool IsPopupVisibleForProfile() const;
  Profile* profile() const { return profile_; }
  virtual void MaybeShowLensPromo();
  virtual void ShowProfilePicker();
  virtual void OnDrivePickerOpened();
  virtual void OnDrivePickerClosed();
  virtual void OnScreensharePickerOpened();
  virtual void OnScreensharePickerClosed();
  using RegionSelectedCallback =
      base::OnceCallback<void(const SkBitmap& result_bitmap)>;
  virtual void ShowRegionSelectOverlay(const SkBitmap& screenshot,
                                       const RegionCaptureSource& source,
                                       RegionSelectedCallback callback);
  virtual void OnFileChooserOpened();
  virtual void OnFileChooserClosed();
  void OpenUrl(const GURL& url,
               WindowOpenDisposition disposition,
               ui::PageTransition transition);
  virtual void OpenUrl(const GURL& url,
                       WindowOpenDisposition disposition,
                       ui::PageTransition transition,
                       base::OnceCallback<void(content::NavigationHandle&)>
                           navigation_handle_callback);

  // Acquires a ScopedProfileKeepAlive for this profile while the popup widget
  // is active or being shown. Returns true if profile keep alive was acquired
  // successfully, false otherwise.
  bool AcquireProfileKeepAlive();

  // Releases the ScopedProfileKeepAlive when the popup widget is hidden or
  // closed.
  void ReleaseProfileKeepAlive();

  // KeyedService:
  void Shutdown() override;

 private:
  omnibox_everywhere::OmniboxEverywhereController* controller() const;
  omnibox_everywhere::OmniboxEverywhereUIManager* ui_manager() const;

  raw_ptr<Profile> profile_;
  std::unique_ptr<omnibox_everywhere::OmniboxEverywhereFeaturePromoController>
      feature_promo_controller_;

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  base::WeakPtrFactory<OmniboxEverywhereService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_
