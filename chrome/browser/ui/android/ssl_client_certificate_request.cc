// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "jni/SSLClientCertificateRequest_jni.h"
#include "net/base/host_port_pair.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/ssl/ssl_platform_key_android.h"
#include "net/ssl/ssl_private_key.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {

namespace {

class SSLClientCertPendingRequests;

const char kSSLClientCertPendingRequests[] = "SSLClientCertPendingRequests";

class ClientCertRequest {
 public:
  ClientCertRequest(
      base::WeakPtr<SSLClientCertPendingRequests> pending_requests,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : pending_requests_(pending_requests),
        cert_request_info_(cert_request_info),
        delegate_(std::move(delegate)) {}

  ~ClientCertRequest() {}

  void CertificateSelected(scoped_refptr<net::X509Certificate> cert,
                           scoped_refptr<net::SSLPrivateKey> key);

  net::SSLCertRequestInfo* cert_request_info() const {
    return cert_request_info_.get();
  }

 private:
  base::WeakPtr<SSLClientCertPendingRequests> pending_requests_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<content::ClientCertificateDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertRequest);
};

class SSLClientCertPendingRequests : public base::SupportsUserData::Data {
 public:
  explicit SSLClientCertPendingRequests(content::WebContents* web_contents)
      : web_contents_(web_contents), weak_factory_(this) {}
  ~SSLClientCertPendingRequests() override {}

  void AddRequest(std::unique_ptr<ClientCertRequest> request);

  void RequestComplete(net::SSLCertRequestInfo* info,
                       scoped_refptr<net::X509Certificate> cert,
                       scoped_refptr<net::SSLPrivateKey> key);

  base::WeakPtr<SSLClientCertPendingRequests> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void PumpRequests();

  bool active_request_ = false;
  base::queue<std::unique_ptr<ClientCertRequest>> pending_requests_;

  content::WebContents* web_contents_;
  base::WeakPtrFactory<SSLClientCertPendingRequests> weak_factory_;
};

static void StartClientCertificateRequest(
    std::unique_ptr<ClientCertRequest> request,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::WindowAndroid* window = ViewAndroidHelper::FromWebContents(web_contents)
                                  ->GetViewAndroid()
                                  ->GetWindowAndroid();
  DCHECK(window);

  // Build the |key_types| JNI parameter, as a String[]
  std::vector<std::string> key_types;
  for (size_t n = 0; n < request->cert_request_info()->cert_key_types.size();
       ++n) {
    switch (request->cert_request_info()->cert_key_types[n]) {
      case net::CLIENT_CERT_RSA_SIGN:
        key_types.push_back("RSA");
        break;
      case net::CLIENT_CERT_ECDSA_SIGN:
        key_types.push_back("ECDSA");
        break;
      default:
        // Ignore unknown types.
        break;
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> key_types_ref =
      base::android::ToJavaArrayOfStrings(env, key_types);
  if (key_types_ref.is_null()) {
    LOG(ERROR) << "Could not create key types array (String[])";
    return;
  }

  // Build the |encoded_principals| JNI parameter, as a byte[][]
  ScopedJavaLocalRef<jobjectArray> principals_ref =
      base::android::ToJavaArrayOfByteArray(
          env, request->cert_request_info()->cert_authorities);
  if (principals_ref.is_null()) {
    LOG(ERROR) << "Could not create principals array (byte[][])";
    return;
  }

  // Build the |host_name| and |port| JNI parameters, as a String and
  // a jint.
  ScopedJavaLocalRef<jstring> host_name_ref =
      base::android::ConvertUTF8ToJavaString(
          env, request->cert_request_info()->host_and_port.host());

  // Pass the address of the delegate through to Java.
  jlong request_id = reinterpret_cast<intptr_t>(request.get());

  if (!chrome::android::
          Java_SSLClientCertificateRequest_selectClientCertificate(
              env, request_id, window->GetJavaObject(), key_types_ref,
              principals_ref, host_name_ref,
              request->cert_request_info()->host_and_port.port())) {
    return;
  }

  // Ownership was transferred to Java.
  ignore_result(request.release());
}

void SSLClientCertPendingRequests::AddRequest(
    std::unique_ptr<ClientCertRequest> request) {
  pending_requests_.push(std::move(request));
  PumpRequests();
}

void SSLClientCertPendingRequests::RequestComplete(
    net::SSLCertRequestInfo* info,
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> key) {
  active_request_ = false;
  std::string host_and_port = info->host_and_port.ToString();

  base::queue<std::unique_ptr<ClientCertRequest>> new_pending_requests;
  while (!pending_requests_.empty()) {
    std::unique_ptr<ClientCertRequest> next =
        std::move(pending_requests_.front());
    pending_requests_.pop();
    if (host_and_port == next->cert_request_info()->host_and_port.ToString()) {
      next->CertificateSelected(cert, key);
    } else {
      new_pending_requests.push(std::move(next));
    }
  }
  pending_requests_.swap(new_pending_requests);

  PumpRequests();
}

void SSLClientCertPendingRequests::PumpRequests() {
  if (active_request_ || pending_requests_.empty()) {
    return;
  }

  active_request_ = true;
  std::unique_ptr<ClientCertRequest> next =
      std::move(pending_requests_.front());
  pending_requests_.pop();

  StartClientCertificateRequest(std::move(next), web_contents_);
}

void ClientCertRequest::CertificateSelected(
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> key) {
  delegate_->ContinueWithCertificate(cert, key);
  if (pending_requests_) {
    pending_requests_->RequestComplete(cert_request_info(), cert, key);
  }
}

}  // namespace

namespace android {

// Called from JNI on request completion/result.
// |env| is the current thread's JNIEnv.
// |clazz| is the SSLClientCertificateRequest JNI class reference.
// |request_id| is the id passed to
// Java_SSLClientCertificateRequest_selectClientCertificate() in Start().
// |encoded_chain_ref| is a JNI reference to a Java array of byte arrays,
// each item holding a DER-encoded X.509 certificate.
// |private_key_ref| is the platform PrivateKey object JNI reference for
// the client certificate.
// Note: both |encoded_chain_ref| and |private_key_ref| will be NULL if
// the user didn't select a certificate.
static void JNI_SSLClientCertificateRequest_OnSystemRequestCompletion(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jlong request_id,
    const JavaParamRef<jobjectArray>& encoded_chain_ref,
    const JavaParamRef<jobject>& private_key_ref) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Take back ownership of the request object.
  std::unique_ptr<ClientCertRequest> request(
      reinterpret_cast<ClientCertRequest*>(request_id));

  if (encoded_chain_ref == NULL || private_key_ref == NULL) {
    LOG(ERROR) << "No client certificate selected";
    request->CertificateSelected(nullptr, nullptr);
    return;
  }

  // Convert the encoded chain to a vector of strings.
  std::vector<std::string> encoded_chain_strings;
  if (encoded_chain_ref) {
    base::android::JavaArrayOfByteArrayToStringVector(
        env, encoded_chain_ref, &encoded_chain_strings);
  }

  std::vector<base::StringPiece> encoded_chain;
  for (size_t n = 0; n < encoded_chain_strings.size(); ++n)
    encoded_chain.push_back(encoded_chain_strings[n]);

  // Create the X509Certificate object from the encoded chain.
  scoped_refptr<net::X509Certificate> client_cert(
      net::X509Certificate::CreateFromDERCertChain(encoded_chain));
  if (!client_cert.get()) {
    LOG(ERROR) << "Could not decode client certificate chain";
    return;
  }

  // Create an SSLPrivateKey wrapper for the private key JNI reference.
  scoped_refptr<net::SSLPrivateKey> private_key =
      net::WrapJavaPrivateKey(client_cert.get(), private_key_ref);
  if (!private_key) {
    LOG(ERROR) << "Could not create OpenSSL wrapper for private key";
    return;
  }

  request->CertificateSelected(std::move(client_cert), std::move(private_key));
}

static void NotifyClientCertificatesChanged() {
  net::CertDatabase::GetInstance()->NotifyObserversCertDBChanged();
}

static void
JNI_SSLClientCertificateRequest_NotifyClientCertificatesChangedOnIOThread(
    JNIEnv* env,
    const JavaParamRef<jclass>&) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    NotifyClientCertificatesChanged();
  } else {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                             base::Bind(&NotifyClientCertificatesChanged));
  }
}

}  // namespace android

void ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList unused_client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  // TODO(asimjour): This should be removed once we have proper
  // implementation of SSL client certificate selector in VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          contents, vr::UiSuppressedElement::kSslClientCertificate)) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  SSLClientCertPendingRequests* active_requests =
      static_cast<SSLClientCertPendingRequests*>(
          contents->GetUserData(&kSSLClientCertPendingRequests));

  if (active_requests == nullptr) {
    active_requests = new SSLClientCertPendingRequests(contents);
    contents->SetUserData(&kSSLClientCertPendingRequests,
                          base::WrapUnique(active_requests));
  }

  active_requests->AddRequest(std::make_unique<ClientCertRequest>(
      active_requests->GetWeakPtr(), cert_request_info, std::move(delegate)));
}

}  // namespace chrome
