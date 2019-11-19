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
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/android/chrome_jni_headers/SSLClientCertificateRequest_jni.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
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

class ClientCertRequest {
 public:
  ClientCertRequest(
      base::WeakPtr<SSLClientCertPendingRequests> pending_requests,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : pending_requests_(pending_requests),
        cert_request_info_(cert_request_info),
        delegate_(std::move(delegate)) {}

  base::OnceClosure GetCancellationCallback() {
    return base::BindOnce(&ClientCertRequest::OnCancel,
                          weak_factory_.GetWeakPtr());
  }

  void CertificateSelected(scoped_refptr<net::X509Certificate> cert,
                           scoped_refptr<net::SSLPrivateKey> key);

  void OnCancel();

  net::SSLCertRequestInfo* cert_request_info() const {
    return cert_request_info_.get();
  }

 private:
  base::WeakPtr<SSLClientCertPendingRequests> pending_requests_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<content::ClientCertificateDelegate> delegate_;
  base::WeakPtrFactory<ClientCertRequest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientCertRequest);
};

class SSLClientCertPendingRequests
    : public content::WebContentsUserData<SSLClientCertPendingRequests>,
      public content::WebContentsObserver {
 public:
  explicit SSLClientCertPendingRequests(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~SSLClientCertPendingRequests() override {}

  void AddRequest(std::unique_ptr<ClientCertRequest> request);

  void RequestComplete(net::SSLCertRequestInfo* info,
                       scoped_refptr<net::X509Certificate> cert,
                       scoped_refptr<net::SSLPrivateKey> key);

  // Remove pending requests when |should_keep| returns false. Calls |on_drop|
  // before dropping a request.
  void FilterPendingRequests(
      std::function<bool(ClientCertRequest*)> should_keep,
      std::function<void(ClientCertRequest*)> on_drop);

  base::WeakPtr<SSLClientCertPendingRequests> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  void WebContentsDestroyed() override;

  class CertificateDialogPolicy {
   public:
    // Has the maximum number of cert dialogs been exceeded?
    bool MaxExceeded() { return count_ >= k_max_displayed_dialogs; }
    // Resets counter. Should be called on navigation.
    void ResetCount() {
      // Record sample right before the value is reset. This represents the
      // maximum number of certificate dialogs displayed by sites in the wild.
      UMA_HISTOGRAM_COUNTS_10000(
          "Net.Certificate.ClientCertDialogCount.Android", count_);
      count_ = 0;
    }
    // Increment the counter.
    void IncrementCount() { count_++; }

   private:
    size_t count_ = 0;
    const size_t k_max_displayed_dialogs = 5;
  };

 private:
  void PumpRequests();

  bool active_request_ = false;

  CertificateDialogPolicy dialog_policy_;
  base::queue<std::unique_ptr<ClientCertRequest>> pending_requests_;
  base::WeakPtrFactory<SSLClientCertPendingRequests> weak_factory_{this};

  friend class content::WebContentsUserData<SSLClientCertPendingRequests>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(SSLClientCertPendingRequests)

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
        key_types.push_back("EC");
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

// Note that the default value for |on_drop| is a no-op.
void SSLClientCertPendingRequests::FilterPendingRequests(
    std::function<bool(ClientCertRequest*)> should_keep,
    std::function<void(ClientCertRequest*)> on_drop = [](auto* unused) {}) {
  base::queue<std::unique_ptr<ClientCertRequest>> new_pending_requests;
  while (!pending_requests_.empty()) {
    std::unique_ptr<ClientCertRequest> next =
        std::move(pending_requests_.front());
    pending_requests_.pop();
    if (should_keep(next.get())) {
      new_pending_requests.push(std::move(next));
    } else {
      on_drop(next.get());
    }
  }
  pending_requests_.swap(new_pending_requests);
}

void SSLClientCertPendingRequests::RequestComplete(
    net::SSLCertRequestInfo* info,
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> key) {
  active_request_ = false;

  // Deduplicate pending requests. Only keep pending requests whose host and
  // port differ from those of the completed request.
  const std::string host_and_port = info->host_and_port.ToString();
  auto should_keep = [host_and_port](ClientCertRequest* req) {
                       return host_and_port != req->cert_request_info()->host_and_port.ToString();
                     };
  auto on_drop = [cert, key](ClientCertRequest* req) {
                   req->CertificateSelected(cert, key);
                 };
  FilterPendingRequests(should_keep, on_drop);

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

  // Check if this page is allowed to show any more client cert dialogs.
  if (!dialog_policy_.MaxExceeded()) {
    dialog_policy_.IncrementCount();
    StartClientCertificateRequest(std::move(next), web_contents());
  }
}

void SSLClientCertPendingRequests::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Be careful to only reset the the client certificate dialog counter when the
  // navigation is user-initiated. Note that |HasUserGesture| does not capture
  // browser-initiated navigations. The negation of |IsRendererInitiated| tells
  // us whether the navigation is browser-generated.
  if (navigation_handle->IsInMainFrame() &&
      (navigation_handle->HasUserGesture() ||
       !navigation_handle->IsRendererInitiated())) {
    // Flush any remaining dialogs before resetting the counter.
    auto should_keep = [](auto* req) { return false; };
    FilterPendingRequests(should_keep);
    dialog_policy_.ResetCount();
  }
}

void SSLClientCertPendingRequests::WebContentsDestroyed() {
  // Record UMA sample for last page loaded in WebContents.
  dialog_policy_.ResetCount();
}

void ClientCertRequest::CertificateSelected(
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> key) {
  delegate_->ContinueWithCertificate(cert, key);
  if (pending_requests_) {
    pending_requests_->RequestComplete(cert_request_info(), cert, key);
  }
}

void ClientCertRequest::OnCancel() {
  // When we receive an OnCancel message, we remove this ClientCertRequest from
  // the queue of pending requests.
  auto should_keep = [this](auto* req) { return req != this; };
  if (pending_requests_) {
    pending_requests_->FilterPendingRequests(should_keep);
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
    JNIEnv* env) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    NotifyClientCertificatesChanged();
  } else {
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindOnce(&NotifyClientCertificatesChanged));
  }
}

}  // namespace android

base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList unused_client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  // TODO(asimjour): This should be removed once we have proper
  // implementation of SSL client certificate selector in VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          contents, vr::UiSuppressedElement::kSslClientCertificate)) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  SSLClientCertPendingRequests::CreateForWebContents(contents);
  SSLClientCertPendingRequests* active_requests =
      SSLClientCertPendingRequests::FromWebContents(contents);

  auto client_cert_request = std::make_unique<ClientCertRequest>(
      active_requests->GetWeakPtr(), cert_request_info, std::move(delegate));
  base::OnceClosure cancellation_callback =
      client_cert_request->GetCancellationCallback();
  active_requests->AddRequest(std::move(client_cert_request));
  return cancellation_callback;
}

}  // namespace chrome
