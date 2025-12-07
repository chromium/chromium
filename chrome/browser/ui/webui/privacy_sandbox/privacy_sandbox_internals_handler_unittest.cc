// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace privacy_sandbox_internals {
namespace {
using ::testing::_;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::StartsWith;
using ::testing::StrEq;

using ReadPrefsWithPrefixesResultType = std::vector<
    privacy_sandbox_internals::mojom::PrivacySandboxInternalsPrefPtr>;

static const char kPrefixA[] = "foo.bar";
static const char kPrefixB[] = "bar.foo";
static const char kShortPrefixA[] = "foo";
static std::vector<std::string> kPrefixAPrefNames = {"foo.bar.baz1",
                                                     "foo.bar.baz2"};
static std::vector<std::string> kPrefixBPrefNames = {
    "bar.foo.moo1", "bar.foo.moo2", "bar.foo.moo3"};
static std::vector<std::string> kPrefixCPrefNames = {"moo.foo.baz"};
static const char kPrefAValue[] = "This is a prefix A pref value ";
static const char kPrefBValue[] = "This is a prefix B pref value ";
static const char kPrefCValue[] = "This is a prefix C pref value ";

class PrivacySandboxInternalsHandlerUnitTest : public testing::Test {
 public:
  PrivacySandboxInternalsHandlerUnitTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    auto prefs_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs_->registry());
    RegisterTestPrefs(prefs_->registry());

    TestingProfile::Builder builder;
    builder.SetPrefService(std::move(prefs_));
    profile_ = builder.Build();

    handler_ = std::make_unique<PrivacySandboxInternalsHandler>(
        profile_.get(), remote_.BindNewPipeAndPassReceiver());
  }

  void RegisterTestPrefs(PrefRegistrySimple* registry) {
    for (auto prefName : kPrefixAPrefNames) {
      registry->RegisterStringPref(prefName, kPrefAValue);
    }
    for (auto prefName : kPrefixBPrefNames) {
      registry->RegisterStringPref(prefName, kPrefBValue);
    }
    for (auto prefName : kPrefixCPrefNames) {
      registry->RegisterStringPref(prefName, kPrefCValue);
    }
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<PrivacySandboxInternalsHandler> handler_;
  mojo::Remote<mojom::PageHandler> remote_;

  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(PrivacySandboxInternalsHandlerUnitTest,
       ReadPrefsWithPrefixes_OnlyReturnsPrefsThatStartPrefixes) {
  base::test::TestFuture<ReadPrefsWithPrefixesResultType> future;
  remote_->ReadPrefsWithPrefixes({kPrefixA, kPrefixB}, future.GetCallback());
  auto& cb_data = future.Get();

  EXPECT_THAT(
      cb_data,
      Each(Pointee(Field(
          &privacy_sandbox_internals::mojom::PrivacySandboxInternalsPref::name,
          AnyOf(StartsWith(kPrefixA), StartsWith(kPrefixB))))));
  EXPECT_THAT(cb_data.size(),
              kPrefixAPrefNames.size() + kPrefixBPrefNames.size());
}

TEST_F(PrivacySandboxInternalsHandlerUnitTest,
       ReadPrefsWithPrefixes_HandlesOverlappingPrefixes) {
  base::test::TestFuture<ReadPrefsWithPrefixesResultType> future;
  remote_->ReadPrefsWithPrefixes({kShortPrefixA, kPrefixA},
                                 future.GetCallback());
  auto& cb_data = future.Get();

  EXPECT_THAT(cb_data.size(), kPrefixAPrefNames.size());
}

TEST_F(PrivacySandboxInternalsHandlerUnitTest,
       ReadPrefsWithPrefixes_EmptyStringInPrefixListReportsBadMessage) {
  std::string received_error;
  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  remote_->ReadPrefsWithPrefixes({kPrefixA, ""}, base::DoNothing());
  remote_.FlushForTesting();

  EXPECT_EQ(received_error, "Empty prefixes are invalid.");
}

TEST_F(PrivacySandboxInternalsHandlerUnitTest,
       ReadPrefsWithPrefixes_DuplicatedPrefixInPrefixListReportsBadMessage) {
  std::string received_error;
  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  remote_->ReadPrefsWithPrefixes({kPrefixA, kPrefixB, kPrefixB},
                                 base::DoNothing());
  remote_.FlushForTesting();

  EXPECT_EQ(received_error, "Duplicate prefixes are invalid.");
}

TEST_F(PrivacySandboxInternalsHandlerUnitTest,
       ReadPrefsWithPrefixes_ResultContainsPrefValues) {
  base::test::TestFuture<ReadPrefsWithPrefixesResultType> future;
  remote_->ReadPrefsWithPrefixes({kPrefixA, kPrefixB}, future.GetCallback());
  auto& cb_data = future.Get();

  EXPECT_THAT(
      cb_data,
      Each(Pointee(AnyOf(
          AllOf(Field(&privacy_sandbox_internals::mojom::
                          PrivacySandboxInternalsPref::name,
                      StartsWith(kPrefixA)),
                Field(&privacy_sandbox_internals::mojom::
                          PrivacySandboxInternalsPref::value,
                      Property(&base::Value::GetString, StrEq(kPrefAValue)))),
          AllOf(
              Field(&privacy_sandbox_internals::mojom::
                        PrivacySandboxInternalsPref::name,
                    StartsWith(kPrefixB)),
              Field(&privacy_sandbox_internals::mojom::
                        PrivacySandboxInternalsPref::value,
                    Property(&base::Value::GetString, StrEq(kPrefBValue))))))));
}

}  // namespace
}  // namespace privacy_sandbox_internals
