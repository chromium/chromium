// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/android/connection_info_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/page_info/android/page_info_client.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_delegate.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/page_info/android/jni_headers/ConnectionInfoView_jni.h"

using base::android::CheckException;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

// static
static jlong JNI_ConnectionInfoView_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  DCHECK(web_contents);

  return reinterpret_cast<intptr_t>(
      new ConnectionInfoViewAndroid(env, obj, web_contents));
}

ConnectionInfoViewAndroid::ConnectionInfoViewAndroid(
    JNIEnv* env,
    jobject java_page_info_pop,
    WebContents* web_contents) {
  page_info_client_ = page_info::GetPageInfoClient();
  DCHECK(page_info_client_);

  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry->IsInitialEntry())
    return;

  popup_jobject_.Reset(env, java_page_info_pop);

  presenter_ = std::make_unique<PageInfo>(
      page_info_client_->CreatePageInfoDelegate(web_contents), web_contents,
      nav_entry->GetURL());
  presenter_->InitializeUiState(this, base::DoNothing());
}

ConnectionInfoViewAndroid::~ConnectionInfoViewAndroid() = default;

void ConnectionInfoViewAndroid::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

void ConnectionInfoViewAndroid::ResetCertDecisions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
}

void ConnectionInfoViewAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  {
    int icon_id = page_info_client_->GetJavaResourceId(
        PageInfoUI::GetIdentityIconID(identity_info.identity_status));
    int icon_color_id = page_info_client_->GetJavaResourceId(
        PageInfoUI::GetIdentityIconColorID(identity_info.identity_status));

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
    std::u16string certificate_label;

    // Only show the certificate viewer link if the connection actually used a
    // certificate.
    if (identity_info.identity_status !=
        PageInfo::SITE_IDENTITY_STATUS_NO_CERT) {
      certificate_label =
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_CERT_INFO_BUTTON);
    }

    Java_ConnectionInfoView_addCertificateSection(
        env, popup_jobject_, icon_id, ConvertUTF8ToJavaString(env, headline),
        description, ConvertUTF16ToJavaString(env, certificate_label),
        icon_color_id);

    if (identity_info.show_ssl_decision_revoke_button) {
      std::u16string reset_button_label = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON);
      Java_ConnectionInfoView_addResetCertDecisionsButton(
          env, popup_jobject_,
          ConvertUTF16ToJavaString(env, reset_button_label));
    }
  }

  {
    ScopedJavaLocalRef<jstring> description = ConvertUTF8ToJavaString(
        env, identity_info.connection_status_description);
    Java_ConnectionInfoView_addDescriptionSection(
        env, popup_jobject_, /*iconId=*/0, nullptr, description,
        /*iconColorId=*/0);
  }

  Java_ConnectionInfoView_addMoreInfoLink(
      env, popup_jobject_,
      ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK)));
  Java_ConnectionInfoView_onReady(env, popup_jobject_);
}
