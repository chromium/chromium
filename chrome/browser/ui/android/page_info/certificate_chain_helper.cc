// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/CertificateChainHelper_jni.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

static ScopedJavaLocalRef<jobjectArray>
JNI_CertificateChainHelper_GetCertificateChain(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jobjectArray>();

  scoped_refptr<net::X509Certificate> cert =
      web_contents->GetController().GetVisibleEntry()->GetSSL().certificate;
  if (!cert)
    return ScopedJavaLocalRef<jobjectArray>();

  std::vector<std::string> cert_chain;
  cert_chain.reserve(1 + cert->intermediate_buffers().size());
  cert_chain.emplace_back(
      net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));
  for (const auto& handle : cert->intermediate_buffers()) {
    cert_chain.emplace_back(
        net::x509_util::CryptoBufferAsStringPiece(handle.get()));
  }

  return base::android::ToJavaArrayOfByteArray(env, cert_chain);
}
