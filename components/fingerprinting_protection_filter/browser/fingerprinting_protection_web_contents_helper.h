// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_filter {
enum class ActivationDecision;
enum class LoadPolicy;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

class FingerprintingProtectionObserver;

class FingerprintingProtectionWebContentsHelper
    : public content::WebContentsUserData<
          FingerprintingProtectionWebContentsHelper> {
 public:
  static void CreateForWebContents(content::WebContents* web_contents,
                                   PrefService* pref_service,
                                   privacy_sandbox::TrackingProtectionSettings*
                                       tracking_protection_settings);

  FingerprintingProtectionWebContentsHelper(
      const FingerprintingProtectionWebContentsHelper&) = delete;
  FingerprintingProtectionWebContentsHelper& operator=(
      const FingerprintingProtectionWebContentsHelper&) = delete;

  ~FingerprintingProtectionWebContentsHelper() override;

  // Will be called at the latest in the WillProcessResponse stage from a
  // NavigationThrottle that was registered before the throttle manager's
  // throttles created in MaybeAppendNavigationThrottles().
  void NotifyPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::ActivationDecision& activation_decision);

  // Called in WillStartRequest or WillRedirectRequest stage from a
  // ChildFrameNavigationFilteringThrottle.
  void NotifyChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      subresource_filter::LoadPolicy load_policy);

  void NotifyOnBlockedResources();

  void AddObserver(FingerprintingProtectionObserver* observer);
  void RemoveObserver(FingerprintingProtectionObserver* observer);

  bool is_subresource_blocked() const { return is_subresource_blocked_; }

 private:
  explicit FingerprintingProtectionWebContentsHelper(
      content::WebContents* web_contents,
      PrefService* pref_service,
      privacy_sandbox::TrackingProtectionSettings*
          tracking_protection_settings);
  friend class content::WebContentsUserData<
      FingerprintingProtectionWebContentsHelper>;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  raw_ptr<PrefService> pref_service_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  bool is_subresource_blocked_ = false;
  base::ObserverList<FingerprintingProtectionObserver>::Unchecked
      observer_list_;
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_
