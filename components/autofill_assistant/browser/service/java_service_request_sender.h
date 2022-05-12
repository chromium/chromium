// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_JAVA_SERVICE_REQUEST_SENDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_JAVA_SERVICE_REQUEST_SENDER_H_

#include <memory>
#include <string>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "url/gurl.h"

namespace autofill_assistant {

// TODO(arbesser): Move this to chrome/browser/android, it does not belong into
// components/autofill_assistant.
//
// Thin C++ wrapper around a service request sender implemented in Java.
// Intended for use in integration tests to inject as a mock backend.
class JavaServiceRequestSender : public ServiceRequestSender {
 public:
  JavaServiceRequestSender(
      const base::android::JavaParamRef<jobject>& jservice_request_sender);
  ~JavaServiceRequestSender() override;
  JavaServiceRequestSender(const JavaServiceRequestSender&) = delete;
  JavaServiceRequestSender& operator=(const JavaServiceRequestSender&) = delete;

  void SendRequest(const GURL& url,
                   const std::string& request_body,
                   ServiceRequestSender::AuthMode auth_mode,
                   ResponseCallback callback,
                   RpcType rpc_type) override;

  void OnResponse(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller,
                  jint http_status,
                  const base::android::JavaParamRef<jbyteArray>& jresponse);

  void SetDisableRpcSigning(bool disable_rpc_signing) override;

 private:
  ResponseCallback callback_;
  base::android::ScopedJavaGlobalRef<jobject> jservice_request_sender_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_JAVA_SERVICE_REQUEST_SENDER_H_
