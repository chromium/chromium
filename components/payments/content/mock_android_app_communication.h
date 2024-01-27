// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_ANDROID_APP_COMMUNICATION_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_ANDROID_APP_COMMUNICATION_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/android_app_communication.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserContext;
}

namespace payments {

class MockAndroidAppCommunication : public AndroidAppCommunication {
 public:
  explicit MockAndroidAppCommunication(content::BrowserContext* context);
  ~MockAndroidAppCommunication() override;

  // AndroidAppCommunication implementation:
  MOCK_METHOD(void,
              GetAppDescriptions,
              (const std::string&, GetAppDescriptionsCallback),
              (override));
  MOCK_METHOD(void,
              IsReadyToPay,
              (const std::string& package_name,
               const std::string& service_name,
               (const std::map<std::string, std::set<std::string>>&
                    stringified_method_data),
               const GURL& top_level_origin,
               const GURL& payment_request_origin,
               const std::string& payment_request_id,
               IsReadyToPayCallback callback),
              (override));
  MOCK_METHOD(
      void,
      InvokePaymentApp,
      (const std::string& package_name,
       const std::string& activity_name,
       (const std::map<std::string, std::set<std::string>>&
            stringified_method_data),
       const GURL& top_level_origin,
       const GURL& payment_request_origin,
       const std::string& payment_request_id,
       const base::UnguessableToken& request_token,
       content::WebContents* web_contents,
       const std::optional<base::UnguessableToken>& twa_instance_identifier,
       InvokePaymentAppCallback callback),
      (override));
  MOCK_METHOD(void,
              AbortPaymentApp,
              (const base::UnguessableToken& request_token,
               AbortPaymentAppCallback callback),
              (override));
  MOCK_METHOD(void, SetForTesting, (), (override));
  MOCK_METHOD(void,
              SetAppForTesting,
              (const std::string& package_name,
               const std::string& method,
               const std::string& response),
              (override));

  base::WeakPtr<AndroidAppCommunication> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<AndroidAppCommunication> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_ANDROID_APP_COMMUNICATION_H_
