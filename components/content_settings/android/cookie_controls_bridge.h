// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_ANDROID_COOKIE_CONTROLS_BRIDGE_H_
#define COMPONENTS_CONTENT_SETTINGS_ANDROID_COOKIE_CONTROLS_BRIDGE_H_

#include "base/android/jni_weak_ref.h"
#include "base/scoped_observation.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content_settings {

// Communicates between CookieControlsController (C++ backend) and PageInfoView
// (Java UI).
class CookieControlsBridge : public OldCookieControlsObserver,
                             public CookieControlsObserver {
 public:
  // Creates a CookeControlsBridge for interaction with a
  // CookieControlsController.
  CookieControlsBridge(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents_android,
      const base::android::JavaParamRef<jobject>&
          joriginal_browser_context_handle);

  CookieControlsBridge(const CookieControlsBridge&) = delete;
  CookieControlsBridge& operator=(const CookieControlsBridge&) = delete;

  ~CookieControlsBridge() override;

  void UpdateWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents_android,
      const base::android::JavaParamRef<jobject>&
          joriginal_browser_context_handle);

  // Called by the Java counterpart when it is getting garbage collected.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetThirdPartyCookieBlockingEnabledForSite(JNIEnv* env,
                                                 bool block_cookies);

  void OnUiClosing(JNIEnv* env);

  void OnEntryPointAnimated(JNIEnv* env);

  int GetCookieControlsStatus(JNIEnv* env);

  int GetBreakageConfidenceLevel(JNIEnv* env);

  // OldCookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       int allowed_cookies,
                       int blocked_cookies) override;
  void OnCookiesCountChanged(int allowed_cookies, int blocked_cookies) override;
  void OnStatefulBounceCountChanged(int bounce_count) override;

  // CookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       base::Time expiration) override;
  void OnSitesCountChanged(int allowed_third_party_sites_count,
                           int blocked_third_party_sites_count) override;
  void OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel level) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobject_;
  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  CookieControlsBreakageConfidenceLevel level_ =
      CookieControlsBreakageConfidenceLevel::kUninitialized;
  absl::optional<base::Time> expiration_;
  absl::optional<int> blocked_cookies_;
  absl::optional<int> allowed_cookies_;
  absl::optional<int> blocked_third_party_sites_count_;
  absl::optional<int> allowed_third_party_sites_count_;
  std::unique_ptr<CookieControlsController> controller_;
  base::ScopedObservation<CookieControlsController, OldCookieControlsObserver>
      old_observation_{this};
  base::ScopedObservation<CookieControlsController, CookieControlsObserver>
      observation_{this};
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_ANDROID_COOKIE_CONTROLS_BRIDGE_H_
