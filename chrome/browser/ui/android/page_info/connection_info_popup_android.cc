// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/page_info/connection_info_popup_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ConnectionInfoPopup_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/page_info/page_info.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

// static
static jlong JNI_ConnectionInfoPopup_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  return reinterpret_cast<intptr_t>(
      new ConnectionInfoPopupAndroid(env, obj, web_contents));
}

ConnectionInfoPopupAndroid::ConnectionInfoPopupAndroid(
    JNIEnv* env,
    jobject java_page_info_pop,
    WebContents* web_contents) {
  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry == nullptr)
    return;

  popup_jobject_.Reset(env, java_page_info_pop);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);

  presenter_ = std::make_unique<PageInfo>(
      this, Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents), web_contents,
      nav_entry->GetURL(), helper->GetSecurityLevel(),
      *helper->GetVisibleSecurityState());
}

ConnectionInfoPopupAndroid::~ConnectionInfoPopupAndroid() {
}

void ConnectionInfoPopupAndroid::Destroy(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  delete this;
}

void ConnectionInfoPopupAndroid::ResetCertDecisions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
}

void ConnectionInfoPopupAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  {
    int icon_id = ResourceMapper::MapFromChromiumId(
        PageInfoUI::GetIdentityIconID(identity_info.identity_status));

    // The headline and the certificate dialog link of the site's identity
    // section is only displayed if the site's identity was verified. If the
    // site's identity was verified, then the headline contains the organization
    // name from the provided certificate. If the organization name is not
    // available than the hostname of the site is used instead.
    std::string headline;
    if (identity_info.certificate) {
      headline = identity_info.site_identity;
    }

    ScopedJavaLocalRef<jstring> description = ConvertUTF8ToJavaString(
        env, identity_info.identity_status_description_android);
    base::string16 certificate_label;

    // Only show the certificate viewer link if the connection actually used a
    // certificate.
    if (identity_info.identity_status !=
        PageInfo::SITE_IDENTITY_STATUS_NO_CERT) {
      certificate_label =
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_CERT_INFO_BUTTON);
    }

    Java_ConnectionInfoPopup_addCertificateSection(
        env, popup_jobject_, icon_id, ConvertUTF8ToJavaString(env, headline),
        description, ConvertUTF16ToJavaString(env, certificate_label));

    if (identity_info.show_ssl_decision_revoke_button) {
      base::string16 reset_button_label = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON);
      Java_ConnectionInfoPopup_addResetCertDecisionsButton(
          env, popup_jobject_,
          ConvertUTF16ToJavaString(env, reset_button_label));
    }
  }

  {
    int icon_id = ResourceMapper::MapFromChromiumId(
        PageInfoUI::GetConnectionIconID(identity_info.connection_status));

    ScopedJavaLocalRef<jstring> description = ConvertUTF8ToJavaString(
        env, identity_info.connection_status_description);
    Java_ConnectionInfoPopup_addDescriptionSection(env, popup_jobject_, icon_id,
                                                   nullptr, description);
  }

  Java_ConnectionInfoPopup_addMoreInfoLink(
      env, popup_jobject_,
      ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK)));
  Java_ConnectionInfoPopup_showDialog(env, popup_jobject_);
}

void ConnectionInfoPopupAndroid::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  NOTIMPLEMENTED();
}

void ConnectionInfoPopupAndroid::SetPageFeatureInfo(
    const PageFeatureInfo& info) {
  NOTIMPLEMENTED();
}

void ConnectionInfoPopupAndroid::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  NOTIMPLEMENTED();
}
