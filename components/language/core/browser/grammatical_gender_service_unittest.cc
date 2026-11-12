// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/grammatical_gender_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/language/core/common/language_experiments.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {
namespace {

using Gender = GrammaticalGenderService::Gender;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kEmail[] = "test@gmail.com";
constexpr char kOtherEmail[] = "other@gmail.com";

class FakeObserver : public GrammaticalGenderService::Observer {
 public:
  void Observe(GrammaticalGenderService* service) {
    observation_.Observe(service);
  }

  bool is_observing() const { return observation_.IsObserving(); }

  void OnGrammaticalGenderChanged(Gender gender) override {
    genders_.push_back(gender);
  }

  // Stops observing the service, as required by the Observer contract.
  void OnGrammaticalGenderServiceShutdown() override {
    ++shutdown_count_;
    observation_.Reset();
  }

  const std::vector<Gender>& genders() const { return genders_; }
  int shutdown_count() const { return shutdown_count_; }

 private:
  base::ScopedObservation<GrammaticalGenderService,
                          GrammaticalGenderService::Observer>
      observation_{this};
  std::vector<Gender> genders_;
  int shutdown_count_ = 0;
};

struct GenderCapabilities {
  std::optional<bool> feminine;
  std::optional<bool> masculine;
  std::optional<bool> neuter;
};

// Sets the feature state for a test. This is a separate fixture member so that
// it can be initialized before the IdentityManager is created, in case it
// depends on the feature state when it's created.
class ScopedTestFeatures {
 public:
  ScopedTestFeatures(bool feature_enabled, bool skip_refresh_token_check) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (feature_enabled ? enabled_features : disabled_features)
        .push_back(kGrammaticalGenderLocalization);
    (skip_refresh_token_check ? enabled_features : disabled_features)
        .push_back(switches::kSkipRefreshTokenCheckInIdentityManager);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GrammaticalGenderServiceTest : public testing::Test {
 protected:
  explicit GrammaticalGenderServiceTest(bool feature_enabled = true,
                                        bool skip_refresh_token_check = true)
      : features_(feature_enabled, skip_refresh_token_check) {}

  // Creates the service, and starts observing it. Note that the observer isn't
  // notified of the gender determined during construction.
  void CreateService() {
    service_ = std::make_unique<GrammaticalGenderService>(
        identity_test_env_.identity_manager());
    observer_.Observe(service_.get());
  }

  AccountInfo SignIn() {
    return identity_test_env_.MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSignin);
  }

  void SetCapabilities(AccountInfo account_info,
                       const GenderCapabilities& capabilities) {
    AccountCapabilitiesTestMutator mutator(&account_info);
    if (capabilities.feminine) {
      mutator.set_should_be_addressed_in_feminine_grammatical_gender(
          *capabilities.feminine);
    }
    if (capabilities.masculine) {
      mutator.set_should_be_addressed_in_masculine_grammatical_gender(
          *capabilities.masculine);
    }
    if (capabilities.neuter) {
      mutator.set_should_be_addressed_in_neuter_grammatical_gender(
          *capabilities.neuter);
    }
    identity_test_env_.UpdateAccountInfoForAccount(account_info);
  }

  // Must be initialized before `task_environment_`, so that it outlives any
  // threads that may check features, and before `identity_test_env_`.
  ScopedTestFeatures features_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<GrammaticalGenderService> service_;
  FakeObserver observer_;
};

TEST_F(GrammaticalGenderServiceTest, SignedOut) {
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kOther);
}

TEST_F(GrammaticalGenderServiceTest, Feminine) {
  SetCapabilities(SignIn(), {.feminine = true, .masculine = false});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
}

TEST_F(GrammaticalGenderServiceTest, Masculine) {
  SetCapabilities(SignIn(), {.feminine = false, .masculine = true});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kMasculine);
}

TEST_F(GrammaticalGenderServiceTest, NoGenderPreference) {
  SetCapabilities(SignIn(),
                  {.feminine = false, .masculine = false, .neuter = false});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kOther);
}

TEST_F(GrammaticalGenderServiceTest, Neuter) {
  SetCapabilities(SignIn(),
                  {.feminine = false, .masculine = false, .neuter = true});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kNeuter);
}

TEST_F(GrammaticalGenderServiceTest, FirstTrueGenderWins) {
  SetCapabilities(SignIn(),
                  {.feminine = true, .masculine = true, .neuter = true});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
}

TEST_F(GrammaticalGenderServiceTest, FirstTrueGenderWinsAfterFalse) {
  SetCapabilities(SignIn(),
                  {.feminine = false, .masculine = true, .neuter = true});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kMasculine);
}

class GrammaticalGenderServiceFeatureDisabledTest
    : public GrammaticalGenderServiceTest {
 protected:
  GrammaticalGenderServiceFeatureDisabledTest()
      : GrammaticalGenderServiceTest(/*feature_enabled=*/false) {}
};

TEST_F(GrammaticalGenderServiceFeatureDisabledTest, FeatureDisabled) {
  AccountInfo account_info = SignIn();
  SetCapabilities(account_info, {.feminine = true, .masculine = false});
  CreateService();
  // With the feature disabled, the account's capabilities are ignored.
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kOther);

  // The service doesn't react to account changes.
  SetCapabilities(account_info,
                  {.feminine = false, .masculine = true, .neuter = false});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kOther);
  EXPECT_THAT(observer_.genders(), IsEmpty());
}

TEST_F(GrammaticalGenderServiceTest, WaitsWhileAllCapabilitiesUnknown) {
  AccountInfo account_info = SignIn();
  CreateService();
  // Capabilities haven't been fetched yet, so the gender shouldn't be reset.
  EXPECT_EQ(service_->GetGrammaticalGender(), std::nullopt);
  EXPECT_THAT(observer_.genders(), IsEmpty());

  SetCapabilities(account_info, {.feminine = false, .masculine = true});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kMasculine);
  EXPECT_THAT(observer_.genders(), ElementsAre(Gender::kMasculine));
}

TEST_F(GrammaticalGenderServiceTest, DoesNotWaitForSomeUnknownCapabilities) {
  // Neuter is unknown, but the others are known to be false.
  SetCapabilities(SignIn(), {.feminine = false, .masculine = false});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kOther);
}

TEST_F(GrammaticalGenderServiceTest, KnownTrueCapabilityWithOthersUnknown) {
  AccountInfo account_info = SignIn();
  CreateService();
  SetCapabilities(account_info, {.masculine = true});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kMasculine);
  EXPECT_THAT(observer_.genders(), ElementsAre(Gender::kMasculine));
}

// Signing out isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GrammaticalGenderServiceTest, SignInAndSignOut) {
  CreateService();
  SetCapabilities(SignIn(), {.feminine = true, .masculine = false});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kOther);

  EXPECT_THAT(observer_.genders(),
              ElementsAre(Gender::kFeminine, Gender::kOther));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(GrammaticalGenderServiceTest, DoesNotReapplyUnchangedGender) {
  AccountInfo account_info = SignIn();
  SetCapabilities(account_info, {.feminine = true, .masculine = false});
  CreateService();

  // An account info update with the same gender shouldn't notify observers.
  SetCapabilities(account_info,
                  {.feminine = true, .masculine = false, .neuter = false});
  EXPECT_THAT(observer_.genders(), IsEmpty());
}

TEST_F(GrammaticalGenderServiceTest, Shutdown) {
  AccountInfo account_info = SignIn();
  SetCapabilities(account_info, {.feminine = true, .masculine = false});
  CreateService();
  service_->Shutdown();
  EXPECT_EQ(observer_.shutdown_count(), 1);
  EXPECT_FALSE(observer_.is_observing());

  SetCapabilities(account_info,
                  {.feminine = false, .masculine = true, .neuter = false});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
  EXPECT_THAT(observer_.genders(), IsEmpty());
}

TEST_F(GrammaticalGenderServiceTest, IdentityManagerShutdownNotifiesOnce) {
  CreateService();
  service_->OnIdentityManagerShutdown(identity_test_env_.identity_manager());
  EXPECT_EQ(observer_.shutdown_count(), 0);

  service_->Shutdown();
  EXPECT_EQ(observer_.shutdown_count(), 1);
  EXPECT_FALSE(observer_.is_observing());
}

TEST_F(GrammaticalGenderServiceTest, PrimaryAccountSwitch) {
  SetCapabilities(SignIn(), {.feminine = true, .masculine = false});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);

  // Switch to an account whose capabilities haven't been fetched yet. The last
  // known gender is kept until they are.
  AccountInfo other_account_info =
      identity_test_env_.MakeAccountAvailable(kOtherEmail);
  identity_test_env_.SetPrimaryAccount(kOtherEmail,
                                       signin::ConsentLevel::kSignin);
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
  EXPECT_THAT(observer_.genders(), IsEmpty());

  SetCapabilities(other_account_info, {.feminine = false, .masculine = true});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kMasculine);
  EXPECT_THAT(observer_.genders(), ElementsAre(Gender::kMasculine));
}

TEST_F(GrammaticalGenderServiceTest, IgnoresNonPrimaryAccounts) {
  SetCapabilities(SignIn(), {.feminine = true, .masculine = false});
  CreateService();

  // A non-primary account's capabilities don't affect the gender.
  AccountInfo other_account_info =
      identity_test_env_.MakeAccountAvailable(kOtherEmail);
  SetCapabilities(other_account_info,
                  {.feminine = false, .masculine = true, .neuter = false});
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
  EXPECT_THAT(observer_.genders(), IsEmpty());
}

TEST_F(GrammaticalGenderServiceTest, ExtendedAccountInfoRemoved) {
  identity_test_env_.EnableRemovalOfExtendedAccountInfo();
  AccountInfo account_info = SignIn();
  SetCapabilities(account_info, {.feminine = true, .masculine = false});
  CreateService();

  // Removing the refresh token removes the primary account's extended account
  // info, so its capabilities become unknown, and the last known gender is
  // kept.
  identity_test_env_.RemoveRefreshTokenForPrimaryAccount();
  ASSERT_TRUE(
      identity_test_env_.identity_manager()
          ->FindExtendedAccountInfoByAccountId(account_info.GetAccountId())
          .IsEmpty());
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
  EXPECT_THAT(observer_.genders(), IsEmpty());
}

// With the refresh token check, an account's capabilities are only available
// once it has a refresh token (e.g. after refresh tokens are loaded at
// startup).
class GrammaticalGenderServiceWithRefreshTokenCheckTest
    : public GrammaticalGenderServiceTest {
 protected:
  GrammaticalGenderServiceWithRefreshTokenCheckTest()
      : GrammaticalGenderServiceTest(/*feature_enabled=*/true,
                                     /*skip_refresh_token_check=*/false) {}
};

TEST_F(GrammaticalGenderServiceWithRefreshTokenCheckTest,
       RefreshTokenAvailable) {
  CoreAccountInfo core_account_info = identity_test_env_.SetPrimaryAccount(
      kEmail, signin::ConsentLevel::kSignin);
  SetCapabilities(AccountInfo::Builder(core_account_info).Build(),
                  {.feminine = true, .masculine = false});
  CreateService();
  EXPECT_EQ(service_->GetGrammaticalGender(), std::nullopt);

  identity_test_env_.SetRefreshTokenForPrimaryAccount();
  EXPECT_EQ(service_->GetGrammaticalGender(), Gender::kFeminine);
  EXPECT_THAT(observer_.genders(), ElementsAre(Gender::kFeminine));
}

}  // namespace
}  // namespace language
