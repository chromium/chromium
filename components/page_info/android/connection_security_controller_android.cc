// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/android/connection_security_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/page_info/page_info_ui.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "page_info_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType()
#include "components/page_info/android/jni_headers/PageInfoConnectionSecurityController_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using net::x509_util::CryptoBufferAsStringPiece;

// static
static jlong JNI_PageInfoConnectionSecurityController_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return reinterpret_cast<intptr_t>(
      new ConnectionSecurityControllerAndroid(env, obj, web_contents));
}

ConnectionSecurityControllerAndroid::ConnectionSecurityControllerAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_controller_obj,
    content::WebContents* web_contents) {
  page_info_client_ = page_info::GetPageInfoClient();
  DCHECK(page_info_client_);

  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry->IsInitialEntry()) {
    return;
  }

  controller_jobject_.Reset(env, java_controller_obj);

  presenter_ = std::make_unique<PageInfo>(
      page_info_client_->CreatePageInfoDelegate(web_contents), web_contents,
      nav_entry->GetURL());
}

ConnectionSecurityControllerAndroid::~ConnectionSecurityControllerAndroid() =
    default;

void ConnectionSecurityControllerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void ConnectionSecurityControllerAndroid::LoadIdentityInfo(JNIEnv* env) {
  // InitializeUiState will call PageInfoUI::SetIdentityInfo, which is
  // implemented below to call setSecurityDescription on the Java controller
  // with the relevant identity info.
  presenter_->InitializeUiState(this, base::DoNothing());
}

ScopedJavaLocalRef<jobjectArray> CertToJavaArray(
    JNIEnv* env,
    const scoped_refptr<net::X509Certificate>& cert) {
  std::vector<std::string> cert_chain;
  if (cert) {
    cert_chain.reserve(cert->cert_buffers().size());
    for (const auto& handle : cert->cert_buffers()) {
      cert_chain.emplace_back(CryptoBufferAsStringPiece(handle.get()));
    }
  }
  return base::android::ToJavaArrayOfByteArray(env, cert_chain);
}

void ConnectionSecurityControllerAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);

  int icon_id = page_info_client_->GetJavaResourceId(
      PageInfoUI::GetIdentityIconID(identity_info.identity_status));
  int icon_color_id = page_info_client_->GetJavaResourceId(
      PageInfoUI::GetIdentityIconColorID(identity_info.identity_status));

  scoped_refptr<net::X509Certificate> cert = identity_info.certificate;
  auto cert_chain = CertToJavaArray(env, cert);

  std::u16string qwac_identity;
  // This matches the same check performed in
  // PageInfoSecurityContentView::SetIdentityInfo.
  bool is_1qwac = identity_info.identity_status ==
                      PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT &&
                  identity_info.connection_status ==
                      PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  auto two_qwac_chain = CertToJavaArray(env, identity_info.two_qwac);
  scoped_refptr<net::X509Certificate> qwac_cert;
  if (is_1qwac) {
    qwac_cert = cert;
  } else if (identity_info.two_qwac) {
    qwac_cert = identity_info.two_qwac;
  }
  if (qwac_cert && !qwac_cert->subject().organization_names.empty() &&
      !qwac_cert->subject().country_name.empty()) {
    qwac_identity = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
        base::UTF8ToUTF16(qwac_cert->subject().organization_names[0]),
        base::UTF8ToUTF16(qwac_cert->subject().country_name));
  }

  Java_PageInfoConnectionSecurityController_setSecurityDescription(
      env, controller_jobject_, icon_id, icon_color_id,
      ConvertUTF16ToJavaString(env, security_description->summary),
      ConvertUTF16ToJavaString(env, security_description->details),
      identity_info.show_ssl_decision_revoke_button, cert_chain, is_1qwac,
      two_qwac_chain, ConvertUTF16ToJavaString(env, qwac_identity));
}

void ConnectionSecurityControllerAndroid::ResetCertDecisions(JNIEnv* env) {
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
}

DEFINE_JNI(PageInfoConnectionSecurityController)
