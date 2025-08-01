// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/featured_client.h"

#include <map>
#include <string>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/constants/featured.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::featured {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;

std::string CreateEscapedFilename(const std::string& trial_name,
                                  const std::string& group_name) {
  std::string escaped_trial_name = base::EscapeAllExceptUnreserved(trial_name);
  std::string escaped_group_name = base::EscapeAllExceptUnreserved(group_name);

  return base::StrCat(
      {escaped_trial_name, feature::kTrialGroupSeparator, escaped_group_name});
}

}  // namespace

class FeaturedClientTest : public testing::Test {
 public:
  FeaturedClientTest()
      : bus_(base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options{})),
        path_(dbus::ObjectPath(::featured::kFeaturedServicePath)),
        proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            bus_.get(),
            ::featured::kFeaturedServiceName,
            path_)) {
    // Makes sure `GetObjectProxy()` is called with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::featured::kFeaturedServiceName, path_))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    active_trials_dir_ = dir_.GetPath().Append("active_trials");
    EXPECT_TRUE(base::CreateDirectory(active_trials_dir_));
  }

  FeaturedClientTest(const FeaturedClientTest&) = delete;
  FeaturedClientTest& operator=(const FeaturedClientTest&) = delete;

  ~FeaturedClientTest() override = default;

 protected:
  // Helper method to test FeaturedClient::ParseTrialFilename. Wrapping this
  // logic simplifies the testing logic by allowing us to use a friend class
  // instead of several FRIEND_TEST_ALL_PREFIXES.
  //
  // Callers must initialize FeaturedClient before calling this method.
  bool ParseTrialFilename(const base::FilePath& path,
                          base::FieldTrial::ActiveGroup& active_group) {
    return FeaturedClient::ParseTrialFilename(path, active_group);
  }

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  dbus::ObjectPath path_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;
  base::ScopedTempDir dir_;
  base::FilePath active_trials_dir_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FeaturedClientTest, InitializeSuccess) {
  FeaturedClient::Initialize(bus_.get());

  ASSERT_NE(FeaturedClient::Get(), nullptr);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, NotInitializedGet) {
  ASSERT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, InitializeFakeSuccess) {
  FeaturedClient::InitializeFake();

  ASSERT_NE(FakeFeaturedClient::Get(), nullptr);

  FakeFeaturedClient::Shutdown();

  EXPECT_EQ(FakeFeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, NotInitializedFakeGet) {
  ASSERT_EQ(FakeFeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, HandleSeedFetched_Success) {
  EXPECT_CALL(*proxy_,
              DoCallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke([](dbus::MethodCall* call, int timeout_ms,
                          dbus::MockObjectProxy::ResponseCallback* callback) {
        std::unique_ptr<dbus::Response> response =
            dbus::Response::CreateEmpty();
        std::move(*callback).Run(response.get());
      }));

  FeaturedClient::Initialize(bus_.get());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_TRUE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

// Check that `HandleSeedFetched` runs the callback with a false success value
// if the server (platform) returns an error responses.
TEST_F(FeaturedClientTest, HandleSeedFetched_Failure_ErrorResponse) {
  EXPECT_CALL(*proxy_,
              DoCallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke([](dbus::MethodCall* call, int timeout_ms,
                          dbus::MockObjectProxy::ResponseCallback* callback) {
        // Not setting the serial causes a crash.
        call->SetSerial(123);
        std::unique_ptr<dbus::Response> response =
            dbus::ErrorResponse::FromMethodCall(call, DBUS_ERROR_FAILED,
                                                "test");
        std::move(*callback).Run(response.get());
      }));

  FeaturedClient::Initialize(bus_.get());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_FALSE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

// Check that `HandleSeedFetched` runs the callback with a false success value
// if the method call is unsuccessful (response is a nullptr).
TEST_F(FeaturedClientTest, HandleSeedFetched_Failure_NullResponse) {
  EXPECT_CALL(*proxy_,
              DoCallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke([](dbus::MethodCall* call, int timeout_ms,
                          dbus::MockObjectProxy::ResponseCallback* callback) {
        std::move(*callback).Run(nullptr);
      }));

  FeaturedClient::Initialize(bus_.get());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_FALSE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

// Check that Fake runs `HandleSeedFetched` callback with a false success value
// by default (no expected responses added).
TEST_F(FeaturedClientTest, FakeHandleSeedFetched_InvokeFalseByDefault) {
  FeaturedClient::InitializeFake();
  FakeFeaturedClient* client = FakeFeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_FALSE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 1);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FakeFeaturedClient::Get(), nullptr);
}

// Check that Fake runs `HandleSeedFetched` callback with value added by
// `AddResponse`.
TEST_F(FeaturedClientTest, FakeHandleSeedFetched_InvokeSuccessWhenSet) {
  FeaturedClient::InitializeFake();
  FakeFeaturedClient* client = FakeFeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;
  client->AddResponse(true);

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_TRUE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 1);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FakeFeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, ReadTrialsActivatedBeforeChromeStartup_FilesExist) {
  // Create active trial files before FeaturedClient is initialized.
  EXPECT_TRUE(base::WriteFile(
      active_trials_dir_.Append("test_trial_1,test_group_1"), ""));
  EXPECT_TRUE(base::WriteFile(
      active_trials_dir_.Append("test_trial_2,test_group_2"), ""));

  std::map<std::string, std::string> expected;
  expected.insert({"test_trial_1", "test_group_1"});
  expected.insert({"test_trial_2", "test_group_2"});

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_callback =
      base::BarrierClosure(2, run_loop.QuitClosure());
  std::map<std::string, std::string> actual;
  FeaturedClient::InitializeForTesting(
      bus_.get(), active_trials_dir_,
      base::BindLambdaForTesting(
          [&actual, &barrier_callback](const std::string& trial_name,
                                       const std::string& group_name) {
            actual.insert({trial_name, group_name});
            barrier_callback.Run();
          }));
  run_loop.Run();
  EXPECT_EQ(actual, expected);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest,
       ReadTrialsActivatedBeforeChromeStartup_NoFilesExist) {
  std::map<std::string, std::string> actual_trials;
  FeaturedClient::InitializeForTesting(
      bus_.get(), active_trials_dir_,
      base::BindLambdaForTesting(
          [&actual_trials](const std::string& trial_name,
                           const std::string& group_name) {
            actual_trials.insert({trial_name, group_name});
          }));
  EXPECT_THAT(actual_trials, IsEmpty());

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, ParseTrialFileName_ImproperFilename_MissingGroup) {
  FeaturedClient::InitializeForTesting(bus_.get(), active_trials_dir_,
                                       base::DoNothing());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  base::FieldTrial::ActiveGroup active_group;
  base::FilePath trial_file = active_trials_dir_.Append("test_trial");

  EXPECT_TRUE(base::WriteFile(trial_file, ""));
  EXPECT_FALSE(ParseTrialFilename(trial_file, active_group));

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest,
       ParseTrialFileName_ImproperFilename_MissingSeparator) {
  FeaturedClient::InitializeForTesting(bus_.get(), active_trials_dir_,
                                       base::DoNothing());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  base::FieldTrial::ActiveGroup active_group;
  base::FilePath trial_file = active_trials_dir_.Append("test_trialtest_group");

  EXPECT_TRUE(base::WriteFile(trial_file, ""));
  EXPECT_FALSE(ParseTrialFilename(trial_file, active_group));

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

struct ParseProperFilenameTestParams {
  std::string expected_trial_name;
  std::string expected_group_name;
};

// Parameterized tests to check that properly formatted filenames with special
// characters (eg. whitespace, /, *, etc) are parsed correctly.
class FeaturedClientTrialFileTest
    : public FeaturedClientTest,
      public ::testing::WithParamInterface<ParseProperFilenameTestParams> {};

TEST_P(FeaturedClientTrialFileTest, ParseProperFilename) {
  FeaturedClient::InitializeForTesting(bus_.get(), active_trials_dir_,
                                       base::DoNothing());
  FeaturedClient* client = FeaturedClient::Get();
  ASSERT_NE(client, nullptr);

  const ParseProperFilenameTestParams test_case = GetParam();
  base::FilePath trial_file = active_trials_dir_.Append(CreateEscapedFilename(
      test_case.expected_trial_name, test_case.expected_group_name));
  EXPECT_TRUE(base::WriteFile(trial_file, ""));

  base::FieldTrial::ActiveGroup active_group;
  EXPECT_TRUE(ParseTrialFilename(trial_file, active_group));
  EXPECT_EQ(active_group.trial_name, test_case.expected_trial_name);
  EXPECT_EQ(active_group.group_name, test_case.expected_group_name);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    FeaturedClientTrialFileTestSuite,
    FeaturedClientTrialFileTest,
    testing::ValuesIn<ParseProperFilenameTestParams>({
        {"test_trial", "test_group"}, {",", "test_group"}, {"/", "test_group"},
        {"&", "test_group"},          {"!", "test_group"}, {"@", "test_group"},
        {"#", "test_group"},          {"$", "test_group"}, {"%", "test_group"},
        {"^", "test_group"},          {".", "test_group"}, {"~", "test_group"},
        {"-", "test_group"},          {"`", "test_group"}, {"(", "test_group"},
        {")", "test_group"},          {"`", "test_group"}, {"?", "test_group"},
        {"+", "test_group"},          {"=", "test_group"}, {" ", "test_group"},
    }));

#if DCHECK_IS_ON()
using FeaturedClientDeathTest = FeaturedClientTest;
TEST_F(FeaturedClientDeathTest, InitializeFailure_NullBus) {
  EXPECT_DEATH(FeaturedClient::Initialize(nullptr), "");
}

TEST_F(FeaturedClientDeathTest, DoubleInitialize) {
  FeaturedClient::Initialize(bus_.get());

  EXPECT_DEATH(FeaturedClient::Initialize(bus_.get()), "");

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientDeathTest, DoubleInitializeFake) {
  FeaturedClient::InitializeFake();

  EXPECT_DEATH(FeaturedClient::InitializeFake(), "");

  FakeFeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}
#endif  // DCHECK_IS_ON()
}  // namespace ash::featured
