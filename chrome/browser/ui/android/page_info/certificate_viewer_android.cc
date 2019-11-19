// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/CertificateViewer_jni.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/grit/generated_resources.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // For Android, showing the certificate is always handled in Java.
  NOTREACHED();
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertIssuedToText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_SUBJECT_GROUP));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertInfoCommonNameText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_COMMON_NAME_LABEL));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertInfoOrganizationText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_ORGANIZATION_LABEL));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertInfoSerialNumberText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_SERIAL_NUMBER_LABEL));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertInfoOrganizationUnitText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_ORGANIZATIONAL_UNIT_LABEL));
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertIssuedByText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_ISSUER_GROUP));
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertValidityText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_VALIDITY_GROUP));
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertIssuedOnText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_ISSUED_ON_LABEL));
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertExpiresOnText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_EXPIRES_ON_LABEL));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertFingerprintsText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_FINGERPRINTS_GROUP));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertSHA256FingerprintText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL));
}

static ScopedJavaLocalRef<jstring>
JNI_CertificateViewer_GetCertSHA1FingerprintText(JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL));
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertExtensionText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_DETAILS_EXTENSIONS));
}

static ScopedJavaLocalRef<jstring> JNI_CertificateViewer_GetCertSANText(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_X509_SUBJECT_ALT_NAME));
}
