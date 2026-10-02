// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/avatar_toolbar_iph_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace {

class MockToolbarButtonProvider : public ToolbarButtonProvider {
 public:
  MOCK_METHOD(ExtensionsContainerViews*,
              GetExtensionsContainerViews,
              (),
              (override));
  MOCK_METHOD(PinnedToolbarActions*, GetPinnedToolbarActions, (), (override));
  MOCK_METHOD(gfx::Size, GetToolbarButtonSize, (), (const, override));
  MOCK_METHOD(views::BubbleAnchor,
              GetDefaultExtensionDialogAnchor,
              (),
              (override));
  MOCK_METHOD(page_actions::PageActionViewInterface*,
              GetPageActionViewInterface,
              (actions::ActionId),
              (override));
  MOCK_METHOD(AppMenuControl*, GetAppMenuControl, (), (override));
  MOCK_METHOD(gfx::Rect, GetFindBarBoundingBox, (int), (override));
  MOCK_METHOD(void, FocusToolbar, (), (override));
  MOCK_METHOD(views::AccessiblePaneView*,
              GetAsAccessiblePaneView,
              (),
              (override));
  MOCK_METHOD(views::BubbleAnchor,
              GetBubbleAnchor,
              (std::optional<actions::ActionId>),
              (override));
  MOCK_METHOD(views::BubbleAnchor,
              GetPageActionBubbleAnchor,
              (actions::ActionId),
              (override));
  MOCK_METHOD(void, ZoomChangedForActiveTab, (bool), (override));
  MOCK_METHOD(AvatarToolbarButtonInterface*,
              GetAvatarToolbarButtonInterface,
              (),
              (override));
  MOCK_METHOD(ToolbarButton*, GetBackButton, (), (override));
  MOCK_METHOD(ReloadControl*, GetReloadButton, (), (override));
  MOCK_METHOD(ToolbarButton*, GetDownloadButton, (), (override));
  MOCK_METHOD(WebUIToolbarWebView*,
              GetWebUIToolbarViewForTesting,
              (),
              (override));
};

class MockAvatarToolbarButton : public AvatarToolbarButtonInterface {
 public:
  MOCK_METHOD(bool, IsMouseHovered, (), (const, override));
  MOCK_METHOD(bool, HasFocus, (), (const, override));
  MOCK_METHOD(views::DialogDelegate*, GetDialogDelegate, (), (override));
  MOCK_METHOD(void, ButtonPressed, (bool), (override));
  MOCK_METHOD(base::ScopedClosureRunner,
              SetExplicitButtonState,
              (const std::u16string&,
               std::optional<std::u16string>,
               std::optional<base::RepeatingCallback<void(bool)>>,
               bool),
              (override));
  MOCK_METHOD(bool, HasExplicitButtonState, (), (const, override));
  MOCK_METHOD(bool, IsReadyForIPH, (), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void, UpdateIcon, (), (override));
  MOCK_METHOD(void, UpdateText, (), (override));
  MOCK_METHOD(void,
              SetAnnounceCallbackForTesting,
              (base::OnceCallback<void(std::u16string)>),
              (override));
  MOCK_METHOD(void, ClearActiveStateForTesting, (), (override));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MOCK_METHOD(void, ForceShowingPromoForTesting, (), (override));
#endif
};

class MockAppBrowserController : public web_app::AppBrowserController {
 public:
  explicit MockAppBrowserController(BrowserWindowInterface* browser)
      : AppBrowserController(browser, "test_app") {}

  MOCK_METHOD(bool, HasMinimalUiButtons, (), (const, override));
  MOCK_METHOD(ui::ImageModel, GetWindowAppIcon, (), (const, override));
  MOCK_METHOD(ui::ImageModel, GetWindowIcon, (), (const, override));
  MOCK_METHOD(std::u16string, GetAppShortName, (), (const, override));
  MOCK_METHOD(std::u16string, GetFormattedUrlOrigin, (), (const, override));
  MOCK_METHOD(const GURL&, GetAppStartUrl, (), (const, override));
  MOCK_METHOD(bool, IsUrlInAppScope, (const GURL&), (const, override));
};

class MockUserEducationStorageService
    : public user_education::test::TestUserEducationStorageService {
 public:
  MOCK_METHOD(std::optional<user_education::FeaturePromoData>,
              ReadPromoData,
              (const base::Feature&),
              (const, override));
};

class AvatarToolbarIphControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(browser_, GetProfile()).WillByDefault(testing::Return(&profile_));
    ON_CALL(toolbar_button_provider_, GetAvatarToolbarButtonInterface())
        .WillByDefault(testing::Return(&avatar_button_));
    EXPECT_CALL(avatar_button_, IsReadyForIPH()).WillRepeatedly([this] {
      return avatar_ready_;
    });
    toolbar_registration_.emplace(browser_.GetUnownedUserDataHost(),
                                  toolbar_button_provider_);
    controller_ = std::make_unique<AvatarToolbarIphController>(
        browser_, profile_, identity_test_env_.identity_manager(),
        &storage_service_);
  }

  void PreparePromo() {}

  void RequestPromo() { controller_->MaybeShowProfileSwitchIPH(); }

  void ExpectPromo() {
    EXPECT_CALL(user_education_, MaybeShowStartupFeaturePromo(testing::_))
        .WillOnce([](user_education::FeaturePromoParams params) {
          EXPECT_EQ(&params.feature.get(),
                    &feature_engagement::kIPHProfileSwitchFeature);
          return true;
        });
  }

  void ExpectNoProfileSwitchIphForProfile(Profile& profile) {
    controller_.reset();
    AvatarToolbarIphController controller(browser_, profile,
                                          /*identity_manager=*/nullptr,
                                          &storage_service_);
    controller.MaybeShowProfileSwitchIPH();
    task_environment_.FastForwardBy(base::Seconds(2));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::StrictMock<MockUserEducationStorageService> storage_service_;
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  testing::NiceMock<MockToolbarButtonProvider> toolbar_button_provider_;
  testing::StrictMock<MockAvatarToolbarButton> avatar_button_;
  bool avatar_ready_ = true;
  testing::StrictMock<MockBrowserUserEducationInterface> user_education_{
      &browser_};
  std::optional<ui::ScopedUnownedUserData<ToolbarButtonProvider>>
      toolbar_registration_;
  std::unique_ptr<AvatarToolbarIphController> controller_;
};

TEST_F(AvatarToolbarIphControllerTest, DoesNotReadStorageOnInitialization) {
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(AvatarToolbarIphControllerTest, ShowsImmediatelyAfterMinimumDelay) {
  task_environment_.FastForwardBy(base::Seconds(2));
  ExpectPromo();
  controller_->MaybeShowProfileSwitchIPH();
}

TEST_F(AvatarToolbarIphControllerTest, ShowsPasswordManagerAppPromo) {
  testing::NiceMock<MockAppBrowserController> app_controller(&browser_);
  const GURL app_url("chrome://password-manager/");
  ON_CALL(app_controller, GetAppStartUrl())
      .WillByDefault(testing::ReturnRef(app_url));
  EXPECT_CALL(user_education_, MaybeShowStartupFeaturePromo(testing::_))
      .WillOnce([](user_education::FeaturePromoParams params) {
        EXPECT_EQ(&params.feature.get(),
                  &feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature);
        return true;
      });

  controller_->MaybeShowProfileSwitchIPH();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(AvatarToolbarIphControllerTest, DoesNothingWithoutToolbar) {
  toolbar_registration_.reset();
  controller_->MaybeShowProfileSwitchIPH();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(AvatarToolbarIphControllerTest, DoesNothingWithoutAvatarButton) {
  ON_CALL(toolbar_button_provider_, GetAvatarToolbarButtonInterface())
      .WillByDefault(testing::Return(nullptr));
  controller_->MaybeShowProfileSwitchIPH();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(AvatarToolbarIphControllerTest, DoesNotShowForGuest) {
  profile_.SetGuestSession(true);
  ASSERT_TRUE(profile_.IsGuestSession());
  ExpectNoProfileSwitchIphForProfile(profile_);
}

TEST_F(AvatarToolbarIphControllerTest, DoesNotShowForIncognito) {
  Profile* profile = profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(profile->IsIncognitoProfile());
  ExpectNoProfileSwitchIphForProfile(*profile);
}

TEST_F(AvatarToolbarIphControllerTest, DoesNotShowForEnterpriseIsolatedMode) {
  Profile* profile = profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile_metrics::SetBrowserProfileType(
      profile, profile_metrics::BrowserProfileType::kEnterpriseIsolated);
  ASSERT_TRUE(profile->IsEnterpriseIsolatedModeProfile());
  ASSERT_FALSE(profile->IsIncognitoProfile());
  ExpectNoProfileSwitchIphForProfile(*profile);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class SupervisedProfileIphControllerTest
    : public AvatarToolbarIphControllerTest {
 protected:
  SupervisedProfileIphControllerTest() {
    features_.InitAndEnableFeature(
        feature_engagement::kIPHSupervisedUserProfileSigninFeature);
  }

  void SignIn(std::optional<bool> is_supervised) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "child@example.com", signin::ConsentLevel::kSignin);
    account_info =
        AccountInfo::Builder(account_info).SetGivenName("Child").Build();
    if (is_supervised.has_value()) {
      AccountCapabilitiesTestMutator(&account_info)
          .set_is_subject_to_parental_controls(*is_supervised);
    }
    identity_test_env_.UpdateAccountInfoForAccount(account_info);
  }

  void PreparePromo() { SignIn(true); }

  void RequestPromo() {
    controller_->MaybeShowSupervisedUserProfileSignInIPH();
  }

  void ExpectPromo() {
    EXPECT_CALL(user_education_, MaybeShowFeaturePromo(testing::_))
        .WillOnce([](user_education::FeaturePromoParams params) {
          EXPECT_EQ(
              &params.feature.get(),
              &feature_engagement::kIPHSupervisedUserProfileSigninFeature);
          EXPECT_EQ(std::get<std::u16string>(params.title_params), u"Child");
          return true;
        });
  }

  base::test::ScopedFeatureList features_;
};

TEST_F(SupervisedProfileIphControllerTest, DoesNotShowWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      feature_engagement::kIPHSupervisedUserProfileSigninFeature);
  SignIn(true);
  controller_->MaybeShowSupervisedUserProfileSignInIPH();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(SupervisedProfileIphControllerTest, DoesNotShowWhenSignedOut) {
  controller_->MaybeShowSupervisedUserProfileSignInIPH();
  task_environment_.FastForwardBy(base::Seconds(2));
}

class SupervisedProfileIphCapabilityTest
    : public SupervisedProfileIphControllerTest,
      public testing::WithParamInterface<std::optional<bool>> {};

TEST_P(SupervisedProfileIphCapabilityTest, DoesNotShowWithoutSupervision) {
  SignIn(GetParam());
  controller_->MaybeShowSupervisedUserProfileSignInIPH();
  task_environment_.FastForwardBy(base::Seconds(2));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupervisedProfileIphCapabilityTest,
                         testing::Values(std::optional<bool>(false),
                                         std::nullopt));

TEST_F(SupervisedProfileIphControllerTest, RechecksAccountAfterDelay) {
  SignIn(true);
  controller_->MaybeShowSupervisedUserProfileSignInIPH();
  identity_test_env_.ClearPrimaryAccount();
  task_environment_.FastForwardBy(base::Seconds(2));
}

class SignInBenefitsIphControllerTest : public AvatarToolbarIphControllerTest {
 protected:
  SignInBenefitsIphControllerTest() {
    features_.InitWithFeatures(
        {syncer::kReplaceSyncPromosWithSignInPromos,
         feature_engagement::kIPHSignInBenefitsFeature},
        {syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
         feature_engagement::kIPHSignInBenefitsNewSigninFeature});
  }

  void PreparePromo() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "user@example.com", signin::ConsentLevel::kSignin);
  }

  void RequestPromo() { controller_->MaybeShowSignInBenefitsIPH(); }

  void ExpectPromo() {
    EXPECT_CALL(user_education_, MaybeShowStartupFeaturePromo(testing::_))
        .WillOnce([](user_education::FeaturePromoParams params) {
          EXPECT_EQ(&params.feature.get(),
                    &feature_engagement::kIPHSignInBenefitsFeature);
          return true;
        });
  }

  base::test::ScopedFeatureList features_;
};

TEST_F(SignInBenefitsIphControllerTest, DoesNotShowWhenSignedOut) {
  RequestPromo();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(SignInBenefitsIphControllerTest, DoesNotShowForSyncingAccount) {
  identity_test_env_.MakePrimaryAccountAvailable("user@example.com",
                                                 signin::ConsentLevel::kSync);
  RequestPromo();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(SignInBenefitsIphControllerTest, RechecksAccountAfterDelay) {
  PreparePromo();
  RequestPromo();
  identity_test_env_.ClearPrimaryAccount();
  task_environment_.FastForwardBy(base::Seconds(2));
}

class SignInBenefitsMigrationTest
    : public SignInBenefitsIphControllerTest,
      public testing::WithParamInterface<const char*> {};

TEST_P(SignInBenefitsMigrationTest, DoesNotShowForMigratedUser) {
  PreparePromo();
  profile_.GetPrefs()->SetBoolean(GetParam(), true);
  RequestPromo();
  task_environment_.FastForwardBy(base::Seconds(2));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignInBenefitsMigrationTest,
    testing::Values(prefs::kPrimaryAccountSetAfterSigninMigration,
                    kDiceMigrationMigrated));

struct SignInBenefitsPromoTestCase {
  bool enable_legacy;
  bool enable_new_signin;
  int legacy_show_count;
  raw_ptr<const base::Feature> expected_promo;
  bool storage_service_available = true;
};

class SignInBenefitsPromoTest
    : public SignInBenefitsIphControllerTest,
      public testing::WithParamInterface<SignInBenefitsPromoTestCase> {
 protected:
  void SetUp() override {
    const auto& test_case = GetParam();
    promo_features_.InitWithFeatureStates(
        {{syncer::kReplaceSyncPromosWithSignInPromos, test_case.enable_legacy},
         {feature_engagement::kIPHSignInBenefitsFeature,
          test_case.enable_legacy},
         {syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
          test_case.enable_new_signin},
         {feature_engagement::kIPHSignInBenefitsNewSigninFeature,
          test_case.enable_new_signin}});
    SignInBenefitsIphControllerTest::SetUp();
    if (!test_case.storage_service_available) {
      controller_.reset();
      controller_ = std::make_unique<AvatarToolbarIphController>(
          browser_, profile_, identity_test_env_.identity_manager(), nullptr);
    }
  }

  base::test::ScopedFeatureList promo_features_;
};

TEST_P(SignInBenefitsPromoTest, SelectsPromo) {
  const auto& test_case = GetParam();
  PreparePromo();
  user_education::FeaturePromoData data;
  data.show_count = test_case.legacy_show_count;
  storage_service_.SavePromoData(feature_engagement::kIPHSignInBenefitsFeature,
                                 data);
  if (test_case.expected_promo) {
    EXPECT_CALL(user_education_, MaybeShowStartupFeaturePromo(testing::_))
        .WillOnce([&test_case](user_education::FeaturePromoParams params) {
          EXPECT_EQ(&params.feature.get(), test_case.expected_promo);
          return true;
        });
  }
  RequestPromo();
  task_environment_.FastForwardBy(base::Milliseconds(1999));

  if (test_case.enable_new_signin && !test_case.enable_legacy &&
      test_case.storage_service_available) {
    EXPECT_CALL(storage_service_,
                ReadPromoData(testing::Ref(
                    feature_engagement::kIPHSignInBenefitsFeature)))
        .WillOnce(testing::Return(data));
  }
  task_environment_.FastForwardBy(base::Milliseconds(1));
}

TEST_P(SignInBenefitsPromoTest, DoesNotAccessStorageWhenSignedOut) {
  RequestPromo();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_P(SignInBenefitsPromoTest, DoesNotAccessStorageForMigratedUser) {
  PreparePromo();
  for (const char* pref : {prefs::kPrimaryAccountSetAfterSigninMigration,
                           kDiceMigrationMigrated}) {
    profile_.GetPrefs()->SetBoolean(pref, true);
    RequestPromo();
    task_environment_.FastForwardBy(base::Seconds(2));
    profile_.GetPrefs()->SetBoolean(pref, false);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignInBenefitsPromoTest,
    testing::Values(
        SignInBenefitsPromoTestCase{false, false, 0, nullptr},
        SignInBenefitsPromoTestCase{
            true, false, 0, &feature_engagement::kIPHSignInBenefitsFeature},
        SignInBenefitsPromoTestCase{
            false, true, 0,
            &feature_engagement::kIPHSignInBenefitsNewSigninFeature},
        SignInBenefitsPromoTestCase{false, true, 1, nullptr},
        SignInBenefitsPromoTestCase{
            true, true, 1, &feature_engagement::kIPHSignInBenefitsFeature},
        SignInBenefitsPromoTestCase{
            false, true, 1,
            &feature_engagement::kIPHSignInBenefitsNewSigninFeature,
            /*storage_service_available=*/false}));

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

template <typename IphTest>
class AvatarIphLifecycleTest : public IphTest {
 protected:
  void SetUp() override {
    IphTest::SetUp();
    this->PreparePromo();
  }
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
using AvatarIphTestTypes = testing::Types<AvatarToolbarIphControllerTest,
                                          SupervisedProfileIphControllerTest,
                                          SignInBenefitsIphControllerTest>;
#else
using AvatarIphTestTypes = testing::Types<AvatarToolbarIphControllerTest>;
#endif
TYPED_TEST_SUITE(AvatarIphLifecycleTest, AvatarIphTestTypes);

TYPED_TEST(AvatarIphLifecycleTest, ShowsAfterRemainingDelay) {
  this->task_environment_.FastForwardBy(base::Seconds(1));
  this->RequestPromo();
  this->task_environment_.FastForwardBy(base::Milliseconds(999));

  this->ExpectPromo();
  this->task_environment_.FastForwardBy(base::Milliseconds(1));
}

TYPED_TEST(AvatarIphLifecycleTest, DelayStartsAtControllerCreation) {
  this->toolbar_registration_.reset();
  this->task_environment_.FastForwardBy(base::Seconds(10));
  this->toolbar_registration_.emplace(this->browser_.GetUnownedUserDataHost(),
                                      this->toolbar_button_provider_);

  // A toolbar created later does not restart the delay.
  this->ExpectPromo();
  this->RequestPromo();
}

TYPED_TEST(AvatarIphLifecycleTest, CancelsPromoWhenControllerIsDestroyed) {
  EXPECT_EQ(this->controller_.get(),
            AvatarToolbarIphController::From(&this->browser_));
  this->RequestPromo();
  this->controller_.reset();
  EXPECT_EQ(nullptr, AvatarToolbarIphController::From(&this->browser_));
  this->task_environment_.FastForwardBy(base::Seconds(2));
}

TYPED_TEST(AvatarIphLifecycleTest, DoesNotQueuePromoBeforeAvatarIsReady) {
  this->avatar_ready_ = false;
  this->RequestPromo();

  // Becoming ready must not revive a request that was ignored.
  this->avatar_ready_ = true;
  this->task_environment_.FastForwardBy(base::Seconds(2));

  this->ExpectPromo();
  this->RequestPromo();
}

TYPED_TEST(AvatarIphLifecycleTest, SkipsPendingPromoWhenToolbarIsDestroyed) {
  this->RequestPromo();
  this->toolbar_registration_.reset();
  this->task_environment_.FastForwardBy(base::Seconds(2));
}

TYPED_TEST(AvatarIphLifecycleTest, SkipsPendingPromoWhenAvatarIsDestroyed) {
  this->RequestPromo();
  ON_CALL(this->toolbar_button_provider_, GetAvatarToolbarButtonInterface())
      .WillByDefault(testing::Return(nullptr));
  this->task_environment_.FastForwardBy(base::Seconds(2));
}

TYPED_TEST(AvatarIphLifecycleTest, SkipsPendingPromoWhenAvatarIsNotReady) {
  this->RequestPromo();
  this->avatar_ready_ = false;
  this->task_environment_.FastForwardBy(base::Seconds(2));

  // Becoming ready must not revive a request skipped by the delayed task.
  this->avatar_ready_ = true;
  this->task_environment_.FastForwardBy(base::Seconds(2));
  this->ExpectPromo();
  this->RequestPromo();
}

TYPED_TEST(AvatarIphLifecycleTest, ChecksCurrentAvatarForPendingPromo) {
  this->RequestPromo();
  testing::StrictMock<MockAvatarToolbarButton> replacement_avatar;
  ON_CALL(this->toolbar_button_provider_, GetAvatarToolbarButtonInterface())
      .WillByDefault(testing::Return(&replacement_avatar));
  EXPECT_CALL(replacement_avatar, IsReadyForIPH())
      .WillOnce(testing::Return(true));
  this->ExpectPromo();
  this->task_environment_.FastForwardBy(base::Seconds(2));
}

}  // namespace
