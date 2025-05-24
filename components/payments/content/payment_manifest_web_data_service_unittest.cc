// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_web_data_service.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/payments/content/payment_method_manifest_table.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using ::testing::_;

testing::Matcher<BrowserBoundKeyMetadata> EqualBrowserBoundKeyMetadata(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    std::vector<uint8_t> bbk_id) {
  return testing::AllOf(
      testing::Field(
          "passkey", &BrowserBoundKeyMetadata::passkey,
          testing::AllOf(
              testing::Field("credential_id",
                             &BrowserBoundKeyMetadata::
                                 RelyingPartyAndCredentialId::credential_id,
                             credential_id),
              testing::Field("relying_party_id",
                             &BrowserBoundKeyMetadata::
                                 RelyingPartyAndCredentialId::relying_party_id,
                             relying_party_id))),
      testing::Field("browser_bound_key_id",
                     &BrowserBoundKeyMetadata::browser_bound_key_id, bbk_id));
}

class MockWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  ~MockWebDataServiceConsumer() override = default;
  MOCK_METHOD(void,
              OnWebDataServiceRequestDone,
              (WebDataServiceBase::Handle, std::unique_ptr<WDTypedResult>),
              (override));
};

class PaymentManifestWebDataServiceTest : public ::testing::Test {
 public:
  PaymentManifestWebDataServiceTest() {
    os_crypt_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        task_environment_.GetMainThreadTaskRunner();
    web_database_service_ = base::MakeRefCounted<WebDatabaseService>(
        base::FilePath(WebDatabase::kInMemoryPath),
        /*ui_task_runner=*/task_runner,
        /*db_task_runner=*/task_runner);
    web_database_service_->AddTable(
        std::make_unique<PaymentMethodManifestTable>());
    web_database_service_->LoadDatabase(os_crypt_.get());
    payment_manifest_web_data_service_ =
        base::MakeRefCounted<PaymentManifestWebDataService>(
            web_database_service_, task_runner);
    payment_manifest_web_data_service_->Init(base::DoNothing());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  scoped_refptr<PaymentManifestWebDataService>
      payment_manifest_web_data_service_;

  std::unique_ptr<WDTypedResult> RunAndWaitForConsumer(
      base::OnceCallback<WebDataServiceBase::Handle(
          WebDataServiceConsumer* consumer)> action) {
    MockWebDataServiceConsumer mock_web_data_service_consumer;
    base::RepeatingClosure quit_closure = task_environment_.QuitClosure();
    WebDataServiceBase::Handle actual_handle;
    std::unique_ptr<WDTypedResult> actual_result;
    EXPECT_CALL(mock_web_data_service_consumer,
                OnWebDataServiceRequestDone(_, _))
        .WillRepeatedly([&actual_handle, &actual_result, &quit_closure](
                            WebDataServiceBase::Handle handle,
                            std::unique_ptr<WDTypedResult> result) {
          actual_handle = handle;
          actual_result = std::move(result);
          quit_closure.Run();
        });
    WebDataServiceBase::Handle handle =
        std::move(action).Run(&mock_web_data_service_consumer);
    task_environment_.RunUntilQuit();
    EXPECT_EQ(actual_handle, handle);
    return actual_result;
  }

  std::unique_ptr<WDTypedResult> RunAndWaitForCallback(
      base::OnceCallback<
          WebDataServiceBase::Handle(WebDataServiceRequestCallback)> action) {
    base::MockCallback<WebDataServiceRequestCallback>
        mock_web_data_service_request_callback;
    base::RepeatingClosure quit_closure = task_environment_.QuitClosure();
    WebDataServiceBase::Handle actual_handle;
    std::unique_ptr<WDTypedResult> actual_result;
    EXPECT_CALL(mock_web_data_service_request_callback, Run(_, _))
        .WillOnce([&actual_handle, &actual_result, &quit_closure](
                      WebDataServiceBase::Handle handle,
                      std::unique_ptr<WDTypedResult> result) {
          actual_handle = handle;
          actual_result = std::move(result);
          quit_closure.Run();
        });
    WebDataServiceBase::Handle handle =
        std::move(action).Run(mock_web_data_service_request_callback.Get());
    task_environment_.RunUntilQuit();
    EXPECT_EQ(actual_handle, handle);
    return actual_result;
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  scoped_refptr<WebDatabaseService> web_database_service_;
};

TEST_F(PaymentManifestWebDataServiceTest, BrowserBoundKey) {
  const std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  const std::string relying_party_id("relying-party.example");
  const std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});
  MockWebDataServiceConsumer mock_web_data_service_consumer;
  base::test::ScopedRunLoopTimeout scoped_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());
  std::unique_ptr<WDTypedResult> set_bbk_result = RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->SetBrowserBoundKey(
            credential_id, relying_party_id, browser_bound_key_id, consumer);
      }));
  ASSERT_TRUE(set_bbk_result);
  ASSERT_EQ(set_bbk_result->GetType(), WDResultType::BOOL_RESULT);
  EXPECT_TRUE(static_cast<WDResult<bool>*>(set_bbk_result.get())->GetValue());

  // Retrieve the browser bound key id.
  std::unique_ptr<WDTypedResult> get_bbk_result = RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->GetBrowserBoundKey(
            credential_id, relying_party_id, consumer);
      }));

  ASSERT_TRUE(get_bbk_result);
  ASSERT_EQ(get_bbk_result->GetType(), WDResultType::BROWSER_BOUND_KEY);
  EXPECT_EQ(static_cast<WDResult<std::optional<std::vector<uint8_t>>>*>(
                get_bbk_result.get())
                ->GetValue(),
            std::optional(browser_bound_key_id));
}

TEST_F(PaymentManifestWebDataServiceTest, GetAllBrowserBoundKey) {
  const std::vector<uint8_t> credential_id_1({0x01, 0x02, 0x03, 0x04});
  const std::string relying_party_id_1("relying-party.example");
  const std::vector<uint8_t> browser_bound_key_id_1({0x11, 0x12, 0x13, 0x14});
  const std::vector<uint8_t> credential_id_2({0x21, 0x22, 0x23, 0x24});
  const std::string relying_party_id_2("another-relying-party.example");
  const std::vector<uint8_t> browser_bound_key_id_2({0x31, 0x32, 0x33, 0x34});
  RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->SetBrowserBoundKey(
            credential_id_1, relying_party_id_1, browser_bound_key_id_1,
            consumer);
      }));
  RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->SetBrowserBoundKey(
            credential_id_2, relying_party_id_2, browser_bound_key_id_2,
            consumer);
      }));

  std::unique_ptr<WDTypedResult> result =
      RunAndWaitForCallback(base::BindLambdaForTesting(
          [&](WebDataServiceRequestCallback request_callback) {
            return payment_manifest_web_data_service_->GetAllBrowserBoundKeys(
                std::move(request_callback));
          }));

  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetType(), WDResultType::BROWSER_BOUND_KEY_METADATA);
  EXPECT_THAT(
      static_cast<WDResult<std::vector<BrowserBoundKeyMetadata>>*>(result.get())
          ->GetValue(),
      testing::UnorderedElementsAre(
          EqualBrowserBoundKeyMetadata(credential_id_1, relying_party_id_1,
                                       browser_bound_key_id_1),
          EqualBrowserBoundKeyMetadata(credential_id_2, relying_party_id_2,
                                       browser_bound_key_id_2)));
}

TEST_F(PaymentManifestWebDataServiceTest, DeleteBrowserBoundKey) {
  const std::vector<uint8_t> credential_id_1({0x01, 0x02, 0x03, 0x04});
  const std::string relying_party_id_1("relying-party.example");
  const std::vector<uint8_t> browser_bound_key_id_1({0x11, 0x12, 0x13, 0x14});
  const std::vector<uint8_t> credential_id_2({0x21, 0x22, 0x23, 0x24});
  const std::string relying_party_id_2("another-relying-party.example");
  const std::vector<uint8_t> browser_bound_key_id_2({0x21, 0x22, 0x23, 0x24});
  const std::vector<uint8_t> credential_id_3({0x41, 0x42, 0x43, 0x44});
  const std::string relying_party_id_3("yet-another-relying-party.example");
  const std::vector<uint8_t> browser_bound_key_id_3({0x51, 0x52, 0x53, 0x54});
  RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->SetBrowserBoundKey(
            credential_id_1, relying_party_id_1, browser_bound_key_id_1,
            consumer);
      }));
  RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->SetBrowserBoundKey(
            credential_id_2, relying_party_id_2, browser_bound_key_id_2,
            consumer);
      }));
  RunAndWaitForConsumer(
      base::BindLambdaForTesting([&](WebDataServiceConsumer* consumer) {
        return payment_manifest_web_data_service_->SetBrowserBoundKey(
            credential_id_3, relying_party_id_3, browser_bound_key_id_3,
            consumer);
      }));
  base::MockCallback<base::OnceClosure> mock_callback;

  EXPECT_CALL(mock_callback, Run());
  payment_manifest_web_data_service_->DeleteBrowserBoundKeys(
      std::vector<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>{
          BrowserBoundKeyMetadata::RelyingPartyAndCredentialId(
              relying_party_id_1, credential_id_1),
          BrowserBoundKeyMetadata::RelyingPartyAndCredentialId(
              relying_party_id_3, credential_id_3),
      },
      mock_callback.Get());

  std::unique_ptr<WDTypedResult> result;
  std::vector<BrowserBoundKeyMetadata> result_data;
  ASSERT_TRUE(base::test::RunUntil([this, &result, &result_data]() -> bool {
    result = RunAndWaitForCallback(base::BindLambdaForTesting(
        [&](WebDataServiceRequestCallback request_callback) {
          return payment_manifest_web_data_service_->GetAllBrowserBoundKeys(
              std::move(request_callback));
        }));
    if (!result ||
        result->GetType() != WDResultType::BROWSER_BOUND_KEY_METADATA) {
      // Bail if the return types are not correct, and let assertions below
      // fail the test.
      return true;
    }
    result_data =
        static_cast<WDResult<std::vector<BrowserBoundKeyMetadata>>*>(
            result.get())
            ->GetValue();  // GetValue() moves the value out of result.
    // Wait until there is only 1 element being returned.
    return result_data.size() == 1;
  })) << "Timeout waiting for only 1 element to be present.";
  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetType(), WDResultType::BROWSER_BOUND_KEY_METADATA);
  EXPECT_THAT(
      result_data,
      testing::UnorderedElementsAre(EqualBrowserBoundKeyMetadata(
          credential_id_2, relying_party_id_2, browser_bound_key_id_2)));
}

}  // namespace

}  // namespace payments
