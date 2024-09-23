// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_TEST_BASE_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_base.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace autofill::payments {

// Base class for tests for the derived class of `PaymentsNetworkInterfaceBase`.
// This class should only contain infrastructure related methods that helps
// test the client server communication between Chrome and Payments backend.
class PaymentsNetworkInterfaceTestBase {
 public:
  PaymentsNetworkInterfaceTestBase();
  PaymentsNetworkInterfaceTestBase(const PaymentsNetworkInterfaceTestBase&) =
      delete;
  PaymentsNetworkInterfaceTestBase& operator=(
      const PaymentsNetworkInterfaceTestBase&) = delete;
  virtual ~PaymentsNetworkInterfaceTestBase();

  void SetUpTest();

  // Registers a field trial with the specified name and group and an associated
  // google web property variation id.
  void CreateFieldTrialWithId(const std::string& trial_name,
                              const std::string& group_name,
                              int variation_id);

 protected:
  network::TestURLLoaderFactory* factory() { return &test_url_loader_factory_; }

  const std::string& GetUploadData() { return intercepted_body_; }

  bool HasVariationsHeader() { return has_variations_header_; }

  // Issues access token in response to any access token request. This will
  // start the Payments Request which requires the authentication.
  void IssueOAuthToken();
  void ReturnResponse(
      PaymentsNetworkInterfaceBase* payments_network_interface_base,
      int response_code,
      const std::string& response_body);

  void assertIncludedInRequest(std::string field_name_or_value);

  void assertNotIncludedInRequest(std::string field_name_or_value);

  PaymentsAutofillClient::PaymentsRpcResult result_ =
      PaymentsAutofillClient::PaymentsRpcResult::kNone;

  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestPersonalDataManager test_personal_data_;
  signin::IdentityTestEnvironment identity_test_env_;

  net::HttpRequestHeaders intercepted_headers_;
  bool has_variations_header_;
  std::string intercepted_body_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_TEST_BASE_H_
