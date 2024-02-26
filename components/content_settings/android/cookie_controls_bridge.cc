// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/android/cookie_controls_bridge.h"

#include <memory>

#include "components/content_settings/android/content_settings_jni_headers/CookieControlsBridge_jni.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace content_settings {

using base::android::JavaParamRef;

CookieControlsBridge::CookieControlsBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jweb_contents_android,
    const base::android::JavaParamRef<jobject>&
        joriginal_browser_context_handle)
    : jobject_(obj) {
  UpdateWebContents(env, jweb_contents_android,
                    joriginal_browser_context_handle);
}

void CookieControlsBridge::UpdateWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents_android,
    const base::android::JavaParamRef<jobject>&
        joriginal_browser_context_handle) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents_android);

  content::BrowserContext* original_context =
      content::BrowserContextFromJavaHandle(joriginal_browser_context_handle);

  content::BrowserContext* context = web_contents->GetBrowserContext();
  auto* permissions_client = permissions::PermissionsClient::Get();

  observation_.Reset();

  controller_ = std::make_unique<CookieControlsController>(
      permissions_client->GetCookieSettings(context),
      original_context ? permissions_client->GetCookieSettings(original_context)
                       : nullptr,
      permissions_client->GetSettingsMap(context),
      permissions_client->GetTrackingProtectionSettings(context));

  observation_.Observe(controller_.get());
  controller_->Update(web_contents);
}

void CookieControlsBridge::OnStatusChanged(
    bool controls_visible,
    bool protections_on,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration) {
  // Only invoke the callback when there is a change.
  if (controls_visible_ == controls_visible &&
      protections_on_ == protections_on && enforcement_ == enforcement &&
      expiration_ == expiration) {
    return;
  }
  controls_visible_ = controls_visible;
  protections_on_ = protections_on;
  enforcement_ = enforcement;
  expiration_ = expiration;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieControlsBridge_onStatusChanged(
      env, jobject_, static_cast<bool>(controls_visible),
      static_cast<bool>(protections_on), static_cast<int>(enforcement_),
      static_cast<int>(blocking_status),
      expiration.InMillisecondsSinceUnixEpoch());
}

void CookieControlsBridge::OnSitesCountChanged(
    int allowed_third_party_sites_count,
    int blocked_third_party_sites_count) {
  // The site counts change quite frequently, so avoid unnecessary
  // UI updates if possible.
  if (allowed_third_party_sites_count_ == allowed_third_party_sites_count &&
      blocked_third_party_sites_count_ == blocked_third_party_sites_count) {
    return;
  }
  allowed_third_party_sites_count_ = allowed_third_party_sites_count;
  blocked_third_party_sites_count_ = blocked_third_party_sites_count;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieControlsBridge_onSitesCountChanged(
      env, jobject_, allowed_third_party_sites_count,
      blocked_third_party_sites_count);
}

void CookieControlsBridge::OnCookieControlsIconStatusChanged(
    bool icon_visible,
    bool protections_on,
    CookieBlocking3pcdStatus blocking_status,
    bool should_highlight) {
  // This function's main use is for web's User Bypass icon, which
  // does not observe `OnStatusChanged`. Since the Clank icon does
  // observe `OnStatusChanged`, the only variable we need to pass
  // on from this function is `should_highlight`.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieControlsBridge_onHighlightCookieControl(
      env, jobject_, static_cast<bool>(should_highlight));
}

void CookieControlsBridge::SetThirdPartyCookieBlockingEnabledForSite(
    JNIEnv* env,
    bool block_cookies) {
  controller_->OnCookieBlockingEnabledForSite(block_cookies);
}

void CookieControlsBridge::OnUiClosing(JNIEnv* env) {
  controller_->OnUiClosing();
}

void CookieControlsBridge::OnEntryPointAnimated(JNIEnv* env) {
  controller_->OnEntryPointAnimated();
}

CookieControlsBridge::~CookieControlsBridge() = default;

void CookieControlsBridge::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean JNI_CookieControlsBridge_IsCookieControlsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  content::BrowserContext* context =
      content::BrowserContextFromJavaHandle(jbrowser_context_handle);
  return permissions::PermissionsClient::Get()
      ->GetCookieSettings(context)
      ->ShouldBlockThirdPartyCookies();
}

static jlong JNI_CookieControlsBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jweb_contents_android,
    const base::android::JavaParamRef<jobject>&
        joriginal_browser_context_handle) {
  return reinterpret_cast<intptr_t>(new CookieControlsBridge(
      env, obj, jweb_contents_android, joriginal_browser_context_handle));
}

}  // namespace content_settings
