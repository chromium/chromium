// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace web_app {

// Data that is attached to a NavigationHandle which describes what app (if any)
// was launched as part of the navigation. When the navigation completes this is
// used to queue launch params, record launch and navigation timing metrics, and
// maybe show navigation capturing IPH.
class WebAppLaunchNavigationHandleUserData
    : public content::NavigationHandleUserData<
          WebAppLaunchNavigationHandleUserData> {
 public:
  ~WebAppLaunchNavigationHandleUserData() override;

  // Static helper for non-navigating launches (e.g., focus existing) to enqueue
  // launch params directly without waiting for navigation to commit.
  static void DispatchLaunchParams(content::WebContents* web_contents,
                                   webapps::LaunchParams launch_params);

  // Note: SetLaunchParams must be called first if both setters are used,
  // because SetLaunchParams overwrites the entire launch_params_ struct,
  // which will clear metadata set by SetLaunchParamsMetadata.
  void SetLaunchParams(webapps::LaunchParams launch_params);
  void SetLaunchParamsMetadata(webapps::AppId app_id,
                               GURL target_url,
                               base::TimeTicks time_navigation_started);

  // Returns the stored launch parameters.
  // CHECKs that the parameters have not already been consumed/moved,
  // which happens if `MaybePerformAppHandlingTasksInWebContents()` has
  // already been called.
  const webapps::LaunchParams& launch_params() const;

  void set_is_navigation_capturing(bool is_navigation_capturing) {
    is_navigation_capturing_ = is_navigation_capturing;
  }
  bool is_navigation_capturing() const { return is_navigation_capturing_; }

  void set_force_iph_off(bool force_iph_off) { force_iph_off_ = force_iph_off; }

  // This method will queue launch params, record launch and navigation timing
  // metrics and maybe show a navigation capturing IPH. Should be called when it
  // is known that this navigation will commit.
  void MaybePerformAppHandlingTasksInWebContents();

 private:
  explicit WebAppLaunchNavigationHandleUserData(
      content::NavigationHandle& navigation_handle);

  friend NavigationHandleUserData;

  raw_ref<content::NavigationHandle> navigation_handle_;
  std::optional<webapps::LaunchParams> launch_params_;
  bool force_iph_off_;
  bool is_navigation_capturing_ = false;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
