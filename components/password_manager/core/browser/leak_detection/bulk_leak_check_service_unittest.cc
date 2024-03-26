// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

const int64_t kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
constexpr char16_t kUsername[] = u"user";
constexpr char16_t kPassword[] = u"password123";

MATCHER_P(CredentialsAre, credentials, "") {
  return base::ranges::equal(arg, credentials.get(),
                             [](const auto& lhs, const auto& rhs) {
                               return lhs.username() == rhs.username() &&
                                      lhs.password() == rhs.password();
                             });
  ;
}

MATCHER_P(CredentialIs, credential, "") {
  return arg.username() == credential.get().username() &&
         arg.password() == credential.get().password();
}

LeakCheckCredential TestCredential() {
  return LeakCheckCredential(kUsername, kPassword);
}

std::vector<LeakCheckCredential> TestCredentials() {
  std::vector<LeakCheckCredential> result;
  result.push_back(TestCredential());
  result.push_back(TestCredential());
  return result;
}

class MockBulkLeakCheck : public BulkLeakCheck {
 public:
  MOCK_METHOD(void,
              CheckCredentials,
              (LeakDetectionInitiator, std::vector<LeakCheckCredential>),
              (override));
  MOCK_METHOD(size_t, GetPendingChecksCount, (), (const override));
};

class MockObserver : public BulkLeakCheckService::Observer {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (BulkLeakCheckService::State state),
              (override));
  MOCK_METHOD(void,
              OnCredentialDone,
              (const LeakCheckCredential& credential, IsLeaked is_leaked),
              (override));
};

class BulkLeakCheckServiceTest : public testing::Test {
 public:
  BulkLeakCheckServiceTest()
      : service_(identity_test_env_.identity_manager(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    auto factory = std::make_unique<MockLeakDetectionCheckFactory>();
    factory_ = factory.get();
    service_.set_leak_factory(std::move(factory));
  }
  ~BulkLeakCheckServiceTest() override { service_.Shutdown(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  BulkLeakCheckService& service() { return service_; }
  MockLeakDetectionCheckFactory& factory() { return *factory_; }

  // Checks |credentials| and simulates its finish. |is_leaked| signifies if one
  // of the credential pretends to be leaked.
  void ConductLeakCheck(std::vector<LeakCheckCredential> credentials,
                        IsLeaked is_leaked);

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::HistogramTester histogram_tester_;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  BulkLeakCheckService service_;
  raw_ptr<MockLeakDetectionCheckFactory> factory_;
};

void BulkLeakCheckServiceTest::ConductLeakCheck(
    std::vector<LeakCheckCredential> credentials,
    IsLeaked is_leaked) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(*leak_check, CheckCredentials);
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));

  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(0));
  delegate->OnFinishedCredential(TestCredential(), is_leaked);
  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());
}

TEST_F(BulkLeakCheckServiceTest, OnCreation) {
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());

  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.BulkCheck"),
      IsEmpty());
}

TEST_F(BulkLeakCheckServiceTest, StartWithZeroPasswords) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, {});

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.BulkCheck"),
      IsEmpty());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, Running) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  auto* weak_leak_check = leak_check.get();
  const std::vector<LeakCheckCredential> credentials = TestCredentials();
  EXPECT_CALL(
      *weak_leak_check,
      CheckCredentials(LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
                       CredentialsAre(std::cref(credentials))));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kRunning, service().GetState());
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(10));
  EXPECT_EQ(10u, service().GetPendingChecksCount());
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.BulkCheck"),
      IsEmpty());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, AppendRunning) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  EXPECT_CALL(*weak_leak_check, CheckCredentials);
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  const std::vector<LeakCheckCredential> credentials = TestCredentials();
  EXPECT_CALL(
      *weak_leak_check,
      CheckCredentials(LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
                       CredentialsAre(std::cref(credentials))));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kRunning, service().GetState());
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(20));
  EXPECT_EQ(20u, service().GetPendingChecksCount());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, FailedToCreateCheck) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(nullptr)));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.BulkCheck"),
      IsEmpty());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, FailedToCreateCheckWithError) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(WithArg<0>([](BulkLeakCheckDelegateInterface* delegate) {
        delegate->OnError(LeakDetectionError::kNotSignIn);
        return nullptr;
      }));
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kSignedOut));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  EXPECT_EQ(BulkLeakCheckService::State::kSignedOut, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  histogram_tester().ExpectUniqueSample("PasswordManager.BulkCheck.Error",
                                        LeakDetectionError::kNotSignIn, 1);

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, CancelNothing) {
  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);

  service().Cancel();

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.BulkCheck"),
      IsEmpty());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, CancelSomething) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));

  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  // Finish one credential before the bulk check gets canceled.
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(true));

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kCanceled));
  service().Cancel();

  EXPECT_EQ(BulkLeakCheckService::State::kCanceled, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, NotifyAboutLeak) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(*leak_check, GetPendingChecksCount).WillRepeatedly(Return(10));
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  LeakCheckCredential credential = TestCredential();
  EXPECT_CALL(observer, OnCredentialDone(CredentialIs(std::cref(credential)),
                                         IsLeaked(false)));
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(false));

  EXPECT_CALL(observer, OnCredentialDone(CredentialIs(std::cref(credential)),
                                         IsLeaked(true)));
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(true));
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix("PasswordManager.BulkCheck"),
      IsEmpty());

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, CheckFinished) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(*leak_check, CheckCredentials);
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(0));
  LeakCheckCredential credential = TestCredential();
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));
  EXPECT_CALL(observer, OnCredentialDone(CredentialIs(std::cref(credential)),
                                         IsLeaked(false)));
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(false));

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.BulkCheck.CheckedCredentials", 2, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.BulkCheck.LeaksFound",
                                        0, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.BulkCheck.Time",
                                        kMockElapsedTime, 1);

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, CheckFinishedWithLeakedCredential) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  MockBulkLeakCheck* weak_leak_check = leak_check.get();
  EXPECT_CALL(*leak_check, CheckCredentials);
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(*weak_leak_check, GetPendingChecksCount)
      .WillRepeatedly(Return(0));
  LeakCheckCredential leaked_credential = TestCredential();
  {
    ::testing::InSequence s;
    EXPECT_CALL(observer,
                OnCredentialDone(CredentialIs(std::cref(leaked_credential)),
                                 IsLeaked(true)));
    EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));
  }
  delegate->OnFinishedCredential(TestCredential(), IsLeaked(true));

  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.BulkCheck.CheckedCredentials", 2, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.BulkCheck.LeaksFound",
                                        1, 1);
  histogram_tester().ExpectUniqueSample("PasswordManager.BulkCheck.Time",
                                        kMockElapsedTime, 1);

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, TwoChecksAfterEachOther) {
  {
    base::HistogramTester histogram_tester;
    std::vector<LeakCheckCredential> result;
    result.push_back(TestCredential());
    ConductLeakCheck(std::move(result), IsLeaked(true));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.BulkCheck.CheckedCredentials", 1, 1);
    histogram_tester.ExpectUniqueSample("PasswordManager.BulkCheck.LeaksFound",
                                        1, 1);
    histogram_tester.ExpectUniqueSample("PasswordManager.BulkCheck.Time",
                                        kMockElapsedTime, 1);
  }
  {
    base::HistogramTester histogram_tester;
    ConductLeakCheck(TestCredentials(), IsLeaked(false));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.BulkCheck.CheckedCredentials", 2, 1);
    histogram_tester.ExpectUniqueSample("PasswordManager.BulkCheck.LeaksFound",
                                        0, 1);
    histogram_tester.ExpectUniqueSample("PasswordManager.BulkCheck.Time",
                                        kMockElapsedTime, 1);
  }
}

TEST_F(BulkLeakCheckServiceTest, CheckFinishedWithError) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kServiceError));
  delegate->OnError(LeakDetectionError::kInvalidServerResponse);

  EXPECT_EQ(BulkLeakCheckService::State::kServiceError, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.BulkCheck.Error",
      LeakDetectionError::kInvalidServerResponse, 1);

  service().RemoveObserver(&observer);
}

TEST_F(BulkLeakCheckServiceTest, CheckFinishedWithQuotaLimit) {
  auto leak_check = std::make_unique<MockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  BulkLeakCheckDelegateInterface* delegate = nullptr;
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(
          DoAll(SaveArg<0>(&delegate), Return(ByMove(std::move(leak_check)))));
  service().CheckUsernamePasswordPairs(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, TestCredentials());

  StrictMock<MockObserver> observer;
  service().AddObserver(&observer);
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kQuotaLimit));
  delegate->OnError(LeakDetectionError::kQuotaLimit);

  EXPECT_EQ(BulkLeakCheckService::State::kQuotaLimit, service().GetState());
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  histogram_tester().ExpectUniqueSample("PasswordManager.BulkCheck.Error",
                                        LeakDetectionError::kQuotaLimit, 1);

  service().RemoveObserver(&observer);
}

}  // namespace
}  // namespace password_manager
