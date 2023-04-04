// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_MANAGER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_client.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_message_controller.h"
#include "url/gurl.h"

namespace webapps {

struct AddToHomescreenParams;
struct InstallableData;
class AppBannerManagerAndroid;

// Coordinates the creation of an install ambient badge, from detecting the
// eligibility to promote the associated web/native app and creating the ambient
// badge. Lifecycle: This class is owned by the AppBannerManagerAndroidclass and
// is instantiated when an ambient badge may be shown.
class AmbientBadgeManager : public InstallableAmbientBadgeClient {
 public:
  explicit AmbientBadgeManager(
      content::WebContents* web_contents,
      base::WeakPtr<AppBannerManagerAndroid> app_banner_manager);

  AmbientBadgeManager(const AmbientBadgeManager&) = delete;
  AmbientBadgeManager& operator=(const AmbientBadgeManager&) = delete;
  ~AmbientBadgeManager() override;

  // This enum backs a UMA histogram , so it should be treated as append-only.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.banners
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AmbientBadgeState
  enum class State {
    // The ambient badge pipeline has not yet been triggered for this page load.
    kInactive = 0,

    // The ambient badge pipeline is running.
    kActive = 1,

    // Ambient badge blocked because of recently dismissed
    kBlocked = 2,

    // Waiting for service worker install to trigger the banner.
    kPendingWorker = 3,

    // Waiting for sufficient engagement to trigger the ambient badge.
    kPendingEngagement = 4,

    // Showing Ambient Badge.
    kShowing = 5,

    // Ambient badge dismissed.
    kDismissed = 6,

    // Ambient badge clicked by the user.
    kClicked = 7,

    // Ambient badge pipeline completed.
    kComplete = 8,

    kMaxValue = kComplete
  };

  State state() const { return state_; }

  void MaybeShow(const GURL& validated_url,
                 const std::u16string& app_name,
                 std::unique_ptr<AddToHomescreenParams> a2hs_params,
                 base::OnceClosure show_banner_callback);

  // InstallableAmbientBadgeClient overrides.
  void AddToHomescreenFromBadge() override;
  void BadgeDismissed() override;

  // Hides the ambient badge if it is showing.
  void HideAmbientBadge();

  // Callback invoked by the InstallableManager once it has finished checking
  // service worker for showing ambient badge.
  void OnWorkerCheckResult(const InstallableData& data);

 protected:
  virtual void UpdateState(State state);

 private:
  // Perform checks and shows the install ambient badge.
  void MaybeShowAmbientBadge();

  void CheckEngagementForAmbientBadge();

  void PerformWorkerCheckForAmbientBadge();

  // Checks whether the web page has sufficient engagement for showing the
  // ambient badge.
  bool HasSufficientEngagementForAmbientBadge();

  // Returns true if it's the first visit and  the badge should be suprressed.
  bool ShouldSuppressAmbientBadge();

  // Called to show UI that promotes installation of a PWA. This is normally the
  // mini-infobar ("banner") but clients can override it by providing a
  // specialization of this class.
  void ShowAmbientBadge();

  // Message controller for the ambient badge.
  InstallableAmbientBadgeMessageController message_controller_{this};

  base::WeakPtr<content::WebContents> web_contents_;
  base::WeakPtr<AppBannerManagerAndroid> app_banner_manager_;

  GURL validated_url_;
  std::u16string app_name_;
  // Contains app parameters such as its type and the install source used.
  std::unique_ptr<AddToHomescreenParams> a2hs_params_;

  base::OnceClosure show_banner_callback_;

  // The current ambient badge status.
  State state_ = State::kInactive;

  bool passed_worker_check_ = false;

  base::WeakPtrFactory<AmbientBadgeManager> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_AMBIENT_BADGE_MANAGER_H_
