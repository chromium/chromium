// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/unguessable_token.h"
#include "components/payments/core/android_app_description.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace payments {

// Invokes Android payment apps. This object is owned by BrowserContext, so it
// should only be accessed on UI thread, where BrowserContext lives.
class AndroidAppCommunication : public base::SupportsUserData::Data {
 public:
  using GetAppDescriptionsCallback = base::OnceCallback<void(
      const std::optional<std::string>& error_message,
      std::vector<std::unique_ptr<AndroidAppDescription>> app_descriptions)>;

  using IsReadyToPayCallback =
      base::OnceCallback<void(const std::optional<std::string>& error_message,
                              bool is_ready_to_pay)>;

  using InvokePaymentAppCallback =
      base::OnceCallback<void(const std::optional<std::string>& error_message,
                              bool is_activity_result_ok,
                              const std::string& payment_method_identifier,
                              const std::string& stringified_details)>;

  using AbortPaymentAppCallback = base::OnceCallback<void(bool)>;

  // Returns a weak pointer to the instance of AndroidAppCommunication that is
  // owned by the given |context|, which should not be null.
  static base::WeakPtr<AndroidAppCommunication> GetForBrowserContext(
      content::BrowserContext* context);

  ~AndroidAppCommunication() override;

  // Disallow copy and assign.
  AndroidAppCommunication(const AndroidAppCommunication& other) = delete;
  AndroidAppCommunication& operator=(const AndroidAppCommunication& other) =
      delete;

  // Looks up installed Android apps that support making payments. If running in
  // TWA mode, the |twa_package_name| parameter is the name of the Android
  // package of the TWA that invoked Chrome, or an empty string otherwise.
  virtual void GetAppDescriptions(const std::string& twa_package_name,
                                  GetAppDescriptionsCallback callback) = 0;

  // Queries the IS_READY_TO_PAY service to check whether the payment app can
  // perform payments.
  virtual void IsReadyToPay(const std::string& package_name,
                            const std::string& service_name,
                            const std::map<std::string, std::set<std::string>>&
                                stringified_method_data,
                            const GURL& top_level_origin,
                            const GURL& payment_request_origin,
                            const std::string& payment_request_id,
                            IsReadyToPayCallback callback) = 0;

  // Invokes the PAY activity to initiate the payment flow.
  virtual void InvokePaymentApp(
      const std::string& package_name,
      const std::string& activity_name,
      const std::map<std::string, std::set<std::string>>&
          stringified_method_data,
      const GURL& top_level_origin,
      const GURL& payment_request_origin,
      const std::string& payment_request_id,
      const base::UnguessableToken& request_token,
      content::WebContents* web_contents,
      const std::optional<base::UnguessableToken>& twa_instance_identifier,
      InvokePaymentAppCallback callback) = 0;

  // Aborts a payment flow which was previously started with InvokePaymentApp().
  virtual void AbortPaymentApp(const base::UnguessableToken& request_token,
                               AbortPaymentAppCallback callback) = 0;

  // Allows usage of a test browser context.
  virtual void SetForTesting() = 0;

  // Simulates having this payment app.
  virtual void SetAppForTesting(const std::string& package_name,
                                const std::string& method,
                                const std::string& response) = 0;

 protected:
  explicit AndroidAppCommunication(content::BrowserContext* context);

  content::BrowserContext* context() { return context_; }

 private:
  // Defined in platform-specific implementation files. See:
  // components/payments/content/android_app_communication_chromeos.cc
  // components/payments/content/android_app_communication_stub.cc
  static std::unique_ptr<AndroidAppCommunication> Create(
      content::BrowserContext* context);

  // Owns this object, so always valid.
  raw_ptr<content::BrowserContext> context_;

  base::WeakPtrFactory<AndroidAppCommunication> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_H_
