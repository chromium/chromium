// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/full_card_requester.h"

#include "base/memory/ref_counted.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Property;
using ::testing::WithArg;

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MOCK_METHOD(
      void,
      ShowUnmaskPrompt,
      (const autofill::CreditCard& card,
       const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
       base::WeakPtr<autofill::CardUnmaskDelegate> delegate),
      (override));
};

class FullCardRequesterTest : public testing::Test {
 public:
  FullCardRequesterTest() = default;

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    autofill_client_.SetPrefs(autofill::test::PrefServiceForTesting());
    autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents_.get(), &autofill_client_,
        base::BindRepeating(&autofill::BrowserDriverInitHook, &autofill_client_,
                            "en-US"));
    autofill_client_.set_test_payments_client(
        std::make_unique<autofill::payments::TestPaymentsClient>(
            test_url_loader_factory_.GetSafeWeakWrapper(),
            autofill_client_.GetIdentityManager(), &personal_data_manager_));
    // Navigate to a site so that the ContentAutofillDriverFactory creates a
    // ContentAutofillDriver for the frame.
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  MockAutofillClient autofill_client_;  // Needs to outlive the web_contents_.
  std::unique_ptr<content::WebContents> web_contents_;
  ::network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<autofill::AutofillWebDataService> database_;
  MockPersonalDataManager personal_data_manager_;
  FullCardRequester full_card_requester_;
  base::MockCallback<
      base::OnceCallback<void(const ClientStatus& status,
                              std::unique_ptr<autofill::CreditCard> card,
                              const std::u16string& cvc)>>
      mock_callback_;
};

TEST_F(FullCardRequesterTest, SuccessfulCardRequest) {
  EXPECT_CALL(autofill_client_, ShowUnmaskPrompt(_, _, _))
      .WillOnce(
          WithArg<2>([](base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
            autofill::CardUnmaskDelegate::UserProvidedUnmaskDetails details;
            details.cvc = u"123";
            details.exp_month = u"1";
            details.exp_year = u"2050";
            delegate->OnUnmaskPromptAccepted(details);
          }));
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _,
                  Eq(u"123")));

  autofill::CreditCard result(/* guid= */ std::string(),
                              autofill::test::kEmptyOrigin);
  full_card_requester_.GetFullCard(web_contents_.get(), &result,
                                   mock_callback_.Get());
}

TEST_F(FullCardRequesterTest, ClosedUnmaskPrompt) {
  EXPECT_CALL(autofill_client_, ShowUnmaskPrompt(_, _, _))
      .WillOnce(
          WithArg<2>([](base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
            delegate->OnUnmaskPromptClosed();
          }));
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, GET_FULL_CARD_FAILED),
                  _, Eq(std::u16string())));

  autofill::CreditCard result(/* guid= */ std::string(),
                              autofill::test::kEmptyOrigin);
  full_card_requester_.GetFullCard(web_contents_.get(), &result,
                                   mock_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
