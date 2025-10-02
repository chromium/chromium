// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_CANVAS_INTERVENTIONS_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_CANVAS_INTERVENTIONS_WEB_CONTENTS_HELPER_H_

#include "base/scoped_observation.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {

// The CanvasInterventionsWebContentsHelper's primary purpose is to listen for
// tabs that have ContentSettingsType::TRACKING_PROTECTION changes for actions
// such as User Bypass. Additionally, the CanvasInterventionsWebContentsHelper
// is used to control the BlockCanvasReadback Runtime Enabled Feature for the
// navigations, based on whether the browser-level feature is enabled and the
// user is in Incognito.
class CanvasInterventionsWebContentsHelper
    : public content::WebContentsUserData<CanvasInterventionsWebContentsHelper>,
      public content::WebContentsObserver,
      public privacy_sandbox::TrackingProtectionSettingsObserver {
 public:
  CanvasInterventionsWebContentsHelper(
      const CanvasInterventionsWebContentsHelper&) = delete;
  CanvasInterventionsWebContentsHelper& operator=(
      const CanvasInterventionsWebContentsHelper&) = delete;

  ~CanvasInterventionsWebContentsHelper() override;

 protected:
  CanvasInterventionsWebContentsHelper(
      content::WebContents* web_contents,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      bool is_incognito);

  // privacy_sandbox::TrackingProtectionSettingsObserver:
  void OnTrackingProtectionExceptionsChanged(
      const GURL& first_party_url) override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<
      CanvasInterventionsWebContentsHelper>;
  bool is_incognito_;

  base::ScopedObservation<privacy_sandbox::TrackingProtectionSettings,
                          privacy_sandbox::TrackingProtectionSettingsObserver>
      tracking_protection_settings_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace fingerprinting_protection_interventions

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_BROWSER_CANVAS_INTERVENTIONS_WEB_CONTENTS_HELPER_H_
