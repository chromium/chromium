// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_TOPIC_SUBSCRIPTION_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_TOPIC_SUBSCRIPTION_REQUEST_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/carrier_lock/common.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  //  namespace network

namespace ash::carrier_lock {

// TopicSubscription request is used to obtain subscription IDs for applications
// that want to use GCM. It requires a set of parameters to be specified to
// identify the Chrome instance, the user, the application and a set of senders
// that will be authorized to address the application using its assigned
// subscription ID.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    TopicSubscriptionRequest {
 public:
  // Defines the common info about a subscription request. All parameters
  // are mandatory.
  struct RequestInfo {
    RequestInfo(uint64_t android_id,
                uint64_t security_token,
                const std::string& app_id,
                const std::string& token,
                const std::string& topic,
                bool unsubscribe = false);
    RequestInfo(const RequestInfo&);
    ~RequestInfo();

    // Android ID of the device.
    uint64_t android_id;
    // Security token of the device.
    uint64_t security_token;

    // FCM application id.
    std::string app_id;
    // FCM sender token.
    std::string token;
    // FCM topic name.
    std::string topic;
    // Set to true for unsubscription.
    bool unsubscribe;
  };

  // Encapsulates the custom logic that is needed to build and process the
  // subscription request.
  TopicSubscriptionRequest(
      const RequestInfo& request_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Callback callback);

  TopicSubscriptionRequest(const TopicSubscriptionRequest&) = delete;
  TopicSubscriptionRequest& operator=(const TopicSubscriptionRequest&) = delete;

  ~TopicSubscriptionRequest();

  void Start();

  // Invoked from SimpleURLLoader.
  void OnUrlLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> body);

 private:
  friend class TopicSubscriptionRequestTest;

  void ReturnResult(Result);

  Callback request_callback_;
  RequestInfo request_info_;
  GURL topic_subscription_url_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<TopicSubscriptionRequest> weak_ptr_factory_{this};
};

}  //  namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_TOPIC_SUBSCRIPTION_REQUEST_H_
