// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

const char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
const char kTestNumber[] = "4234567890123456";  // Visa

std::string NextYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 1);
}

std::string NextMonth() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.month % 12 + 1);
}

}  // namespace

class CreditCardCVCAuthenticatorTest : public testing::Test {
 public:
  CreditCardCVCAuthenticatorTest() {}

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_manager_.Init(/*profile_database=*/database_,
                                /*account_database=*/nullptr,
                                /*pref_service=*/autofill_client_.GetPrefs(),
                                /*identity_manager=*/nullptr,
                                /*client_profile_validator=*/nullptr,
                                /*history_service=*/nullptr,
                                /*is_off_the_record=*/false);
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());

    requester_.reset(new TestAuthenticationRequester());
    autofill_driver_ =
        std::make_unique<testing::NiceMock<TestAutofillDriver>>();

    payments::TestPaymentsClient* payments_client =
        new payments::TestPaymentsClient(
            autofill_driver_->GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client));
    cvc_authenticator_ =
        std::make_unique<CreditCardCVCAuthenticator>(&autofill_client_);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();

    personal_data_manager_.SetPrefService(nullptr);
    personal_data_manager_.ClearCreditCards();
  }

  CreditCard CreateServerCard(std::string guid, std::string number) {
    CreditCard masked_server_card = CreditCard();
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            number.c_str(), NextMonth().c_str(),
                            NextYear().c_str(), "1");
    masked_server_card.set_guid(guid);
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);

    personal_data_manager_.ClearCreditCards();
    personal_data_manager_.AddServerCreditCard(masked_server_card);

    return masked_server_card;
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan) {
    payments::FullCardRequest* full_card_request =
        cvc_authenticator_->full_card_request_.get();
    DCHECK(full_card_request);

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = base::ASCIIToUTF16("123");
    full_card_request->OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::PaymentsClient::UnmaskResponseDetails response;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
  }

 protected:
  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  scoped_refptr<AutofillWebDataService> database_;
  TestPersonalDataManager personal_data_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<CreditCardCVCAuthenticator> cvc_authenticator_;
};

TEST_F(CreditCardCVCAuthenticatorTest, AuthenticateServerCardSuccess) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  cvc_authenticator_->Authenticate(&card, requester_->GetWeakPtr(),
                                   &personal_data_manager_,
                                   AutofillTickClock::NowTicks());

  OnDidGetRealPan(AutofillClient::SUCCESS, kTestNumber);
  EXPECT_TRUE(requester_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), requester_->number());
}

TEST_F(CreditCardCVCAuthenticatorTest, AuthenticateServerCardNetworkError) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  cvc_authenticator_->Authenticate(&card, requester_->GetWeakPtr(),
                                   &personal_data_manager_,
                                   AutofillTickClock::NowTicks());

  OnDidGetRealPan(AutofillClient::NETWORK_ERROR, std::string());
  EXPECT_FALSE(requester_->did_succeed());
}

TEST_F(CreditCardCVCAuthenticatorTest, AuthenticateServerCardPermanentFailure) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  cvc_authenticator_->Authenticate(&card, requester_->GetWeakPtr(),
                                   &personal_data_manager_,
                                   AutofillTickClock::NowTicks());

  OnDidGetRealPan(AutofillClient::PERMANENT_FAILURE, std::string());
  EXPECT_FALSE(requester_->did_succeed());
}

TEST_F(CreditCardCVCAuthenticatorTest, AuthenticateServerCardTryAgainFailure) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  cvc_authenticator_->Authenticate(&card, requester_->GetWeakPtr(),
                                   &personal_data_manager_,
                                   AutofillTickClock::NowTicks());

  OnDidGetRealPan(AutofillClient::TRY_AGAIN_FAILURE, std::string());
  EXPECT_FALSE(requester_->did_succeed());

  OnDidGetRealPan(AutofillClient::SUCCESS, kTestNumber);
  EXPECT_TRUE(requester_->did_succeed());
  EXPECT_EQ(ASCIIToUTF16(kTestNumber), requester_->number());
}

}  // namespace autofill
