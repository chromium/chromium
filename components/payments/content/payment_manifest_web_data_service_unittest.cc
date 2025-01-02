// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_web_data_service.h"

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
  auto quit_closure = task_environment_.QuitClosure();
  WebDataServiceBase::Handle actual_handle;
  std::unique_ptr<WDTypedResult> actual_result;

  // Save the browser bound key id.
  EXPECT_CALL(mock_web_data_service_consumer, OnWebDataServiceRequestDone(_, _))
      .WillRepeatedly([&actual_handle, &actual_result, &quit_closure](
                          WebDataServiceBase::Handle handle,
                          std::unique_ptr<WDTypedResult> result) {
        actual_handle = handle;
        actual_result = std::move(result);
        quit_closure.Run();
      });
  WebDataServiceBase::Handle handle =
      payment_manifest_web_data_service_->SetBrowserBoundKey(
          credential_id, relying_party_id, browser_bound_key_id,
          &mock_web_data_service_consumer);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(actual_handle, handle);
  ASSERT_TRUE(actual_result);
  ASSERT_EQ(actual_result->GetType(), WDResultType::BOOL_RESULT);
  EXPECT_TRUE(static_cast<WDResult<bool>*>(actual_result.get())->GetValue());

  // Reset the closure and output values.
  quit_closure = task_environment_.QuitClosure();
  actual_handle = WebDataServiceBase::Handle();
  actual_result.reset(nullptr);

  // Retrieve the browser bound key id.
  handle = payment_manifest_web_data_service_->GetBrowserBoundKey(
      credential_id, relying_party_id, &mock_web_data_service_consumer);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(actual_handle, handle);
  ASSERT_TRUE(actual_result);
  ASSERT_EQ(actual_result->GetType(), WDResultType::BROWSER_BOUND_KEY);
  EXPECT_EQ(static_cast<WDResult<std::optional<std::vector<uint8_t>>>*>(
                actual_result.get())
                ->GetValue(),
            std::optional(browser_bound_key_id));
}

}  // namespace
}  // namespace payments
