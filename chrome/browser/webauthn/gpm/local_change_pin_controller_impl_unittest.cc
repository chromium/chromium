// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_change_pin_controller_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/gpm_pin_availability.h"
#include "chrome/browser/webauthn/gpm_pin_service.h"
#include "chrome/browser/webauthn/gpm_pin_service_factory.h"
#include "chrome/browser/webauthn/mock_gpm_pin_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "device/fido/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace webauthn {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::StrictMock;
using Step = AuthenticatorRequestDialogModel::Step;

constexpr char kTestEmail[] = "user@example.com";
#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kOtherEmail[] = "other@example.com";
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Observer that re-entrantly triggers CancelAuthenticatorRequest() when the
// dialog transitions to Step::kNotStarted.
class ReentrantCancelOnResetObserver
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit ReentrantCancelOnResetObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    observation_.Observe(model);
  }
  ~ReentrantCancelOnResetObserver() override = default;

  void OnStepTransition() override {
    if (model_->step() == Step::kNotStarted) {
      model_->CancelAuthenticatorRequest();
    }
  }

 private:
  const raw_ptr<AuthenticatorRequestDialogModel> model_;
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      observation_{this};
};

class LocalChangePinControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  LocalChangePinControllerImplTest() = default;
  ~LocalChangePinControllerImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    NavigateAndCommit(GURL("https://passwords.google.com"));
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        GpmPinServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<StrictMock<MockGpmPinService>>();
            }));
    return factories;
  }

 protected:
  CoreAccountInfo SignInPrimaryAccount(const char* email = kTestEmail) {
    return identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(email, signin::ConsentLevel::kSignin)
        .GetCoreAccountInfo();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void ClearPrimaryAccount() {
    identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  LocalChangePinControllerImpl* controller() {
    return LocalChangePinControllerImpl::GetOrCreateForCurrentDocument(
        main_rfh());
  }

  StrictMock<MockGpmPinService>& mock_pin_service() {
    auto* service = static_cast<StrictMock<MockGpmPinService>*>(
        GpmPinServiceFactory::GetForProfile(profile()));
    CHECK(service);
    return *service;
  }

  StrictMock<device_reauth::MockDeviceAuthenticator>* ConfigureOsUv(
      bool available) {
    if (!available) {
      controller()->SetDeviceAuthenticatorFactoryForTesting(base::BindRepeating(
          []() -> std::unique_ptr<device_reauth::DeviceAuthenticator> {
            return nullptr;
          }));
      return nullptr;
    }
    auto authenticator =
        std::make_unique<StrictMock<device_reauth::MockDeviceAuthenticator>>();
    StrictMock<device_reauth::MockDeviceAuthenticator>* raw_authenticator =
        authenticator.get();
    std::unique_ptr<device_reauth::DeviceAuthenticator> base_authenticator =
        std::move(authenticator);
    controller()->SetDeviceAuthenticatorFactoryForTesting(base::BindRepeating(
        [](std::unique_ptr<device_reauth::DeviceAuthenticator>& owned) {
          CHECK(owned);
          return std::move(owned);
        },
        base::OwnedRef(std::move(base_authenticator))));
    return raw_authenticator;
  }

  base::test::ScopedFeatureList feature_list_{
      device::kWebAuthnLocalGpmPasskeys};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(LocalChangePinControllerImplTest,
       IsChangePinFlowAvailableFalseWithoutPrimaryAccount) {
  base::test::TestFuture<bool> future;
  controller()->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       IsChangePinFlowAvailableFalseWhenSyncFails) {
  CoreAccountInfo account = SignInPrimaryAccount();
  LocalChangePinControllerImpl* ctrl = controller();

  EXPECT_CALL(mock_pin_service(), SyncPinState(Eq(account), _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  base::test::TestFuture<bool> future;
  ctrl->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       IsChangePinFlowAvailableFalseWhenPinUnset) {
  CoreAccountInfo account = SignInPrimaryAccount();
  LocalChangePinControllerImpl* ctrl = controller();

  EXPECT_CALL(mock_pin_service(), SyncPinState(Eq(account), _))
      .WillOnce(RunOnceCallback<1>(GpmPinService::PinState{
          .availability = GpmPinAvailability::kGpmPinUnset}));

  base::test::TestFuture<bool> future;
  ctrl->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       IsChangePinFlowAvailableTrueWhenPinUsable) {
  CoreAccountInfo account = SignInPrimaryAccount();
  LocalChangePinControllerImpl* ctrl = controller();

  EXPECT_CALL(mock_pin_service(), SyncPinState(Eq(account), _))
      .WillOnce(RunOnceCallback<1>(GpmPinService::PinState{
          .availability = GpmPinAvailability::kGpmPinSetAndUsable}));

  base::test::TestFuture<bool> future;
  ctrl->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       IsChangePinFlowAvailableTrueWhenPinSetButUnusable) {
  CoreAccountInfo account = SignInPrimaryAccount();
  LocalChangePinControllerImpl* ctrl = controller();

  EXPECT_CALL(mock_pin_service(), SyncPinState(Eq(account), _))
      .WillOnce(RunOnceCallback<1>(GpmPinService::PinState{
          .availability = GpmPinAvailability::kGpmPinSetButNotUsable}));

  base::test::TestFuture<bool> future;
  ctrl->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(LocalChangePinControllerImplTest, OsUvSucceedsAndSetPinSucceeds) {
  CoreAccountInfo account = SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

#if BUILDFLAG(IS_MAC)
  const std::u16string expected_prompt = l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_CHANGE_PIN_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  const std::u16string expected_prompt = l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_CHANGE_PIN_AUTHENTICATION_PROMPT);
#else
  const std::u16string expected_prompt;
#endif

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_callback;
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(Eq(expected_prompt), _))
      .WillOnce(MoveArg<1>(&auth_callback));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  ASSERT_TRUE(auth_callback);
  std::move(auth_callback).Run(true);

  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  EXPECT_EQ(model->step(), Step::kGPMChangePin);

  // Toggle between arbitrary and 6-digit PIN steps via model observer dispatch.
  model->OnGPMPinOptionChanged(true);
  EXPECT_EQ(model->step(), Step::kGPMChangeArbitraryPin);
  model->OnGPMPinOptionChanged(false);
  EXPECT_EQ(model->step(), Step::kGPMChangePin);

  EXPECT_CALL(mock_pin_service(), SetPin(Eq(account), _, _))
      .WillOnce([](const CoreAccountInfo&, GpmPin pin,
                   GpmPinService::SetPinCallback cb) {
        EXPECT_EQ(pin.value().value(), "654321");
        std::move(cb).Run(true);
      });

  model->OnGPMPinEntered(u"654321");
  EXPECT_TRUE(change_future.Get());
  EXPECT_EQ(model->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest, OsUvSucceedsAndSetPinFails) {
  CoreAccountInfo account = SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  EXPECT_CALL(mock_pin_service(), SetPin(Eq(account), _, _))
      .WillOnce(RunOnceCallback<2>(false));

  model->OnGPMPinEntered(u"654321");
  EXPECT_EQ(model->step(), Step::kGPMError);
  EXPECT_FALSE(change_future.IsReady());

  model->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
  EXPECT_EQ(model->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest, OsUvSucceedsAndUserCancelsPinDialog) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  model->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
  EXPECT_EQ(model->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest, OsUvCancelledFailsClosed) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(false));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  EXPECT_FALSE(change_future.Get());
  EXPECT_EQ(controller()->model_for_testing()->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest,
       OsUvUnavailableFailsClosedPendingReauth) {
  SignInPrimaryAccount();
  ConfigureOsUv(/*available=*/false);

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  EXPECT_FALSE(change_future.Get());
  EXPECT_EQ(controller()->model_for_testing()->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest,
       CancelWhileOsUvPendingWithSynchronousCancelCallback) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_callback;
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(MoveArg<1>(&auth_callback));
  // Simulate platform authenticators (e.g. macOS) that synchronously invoke the
  // pending AuthenticateCallback(false) inside Cancel().
  EXPECT_CALL(*authenticator, Cancel()).WillOnce([&auth_callback]() {
    if (auth_callback) {
      std::move(auth_callback).Run(false);
    }
  });

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  EXPECT_FALSE(change_future.IsReady());

  controller()->model_for_testing()->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
  EXPECT_EQ(controller()->model_for_testing()->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest,
       DocumentDestroyedWhileOsUvPendingCancelsCleanly) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_callback;
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(MoveArg<1>(&auth_callback));
  EXPECT_CALL(*authenticator, Cancel()).WillOnce([&auth_callback]() {
    if (auth_callback) {
      std::move(auth_callback).Run(false);
    }
  });

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  EXPECT_FALSE(change_future.IsReady());

  DeleteContents();
  EXPECT_FALSE(change_future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       CancelWhileSetPinInFlightIgnoresLateCallback) {
  CoreAccountInfo account = SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  GpmPinService::SetPinCallback set_pin_callback;
  EXPECT_CALL(mock_pin_service(), SetPin(Eq(account), _, _))
      .WillOnce(MoveArg<2>(&set_pin_callback));

  model->OnGPMPinEntered(u"654321");
  EXPECT_TRUE(model->ui_disabled_);
  ASSERT_TRUE(set_pin_callback);

  model->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
  EXPECT_EQ(model->step(), Step::kNotStarted);
  EXPECT_FALSE(model->ui_disabled_);

  // Late completion from GpmPinService::SetPin must be dropped cleanly.
  std::move(set_pin_callback).Run(true);
  EXPECT_EQ(model->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest,
       DuplicatePinEnteredWhileSetPinInFlightIgnored) {
  CoreAccountInfo account = SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  GpmPinService::SetPinCallback set_pin_callback;
  EXPECT_CALL(mock_pin_service(), SetPin(Eq(account), _, _))
      .WillOnce(MoveArg<2>(&set_pin_callback));

  model->OnGPMPinEntered(u"654321");
  // Duplicate submissions or PIN option changes while SetPin is already in
  // flight must be ignored without re-enabling the UI.
  model->OnGPMPinEntered(u"111111");
  model->OnGPMPinOptionChanged(true);
  EXPECT_EQ(model->step(), Step::kGPMChangePin);
  EXPECT_TRUE(model->ui_disabled_);

  ASSERT_TRUE(set_pin_callback);
  std::move(set_pin_callback).Run(true);
  EXPECT_TRUE(change_future.Get());
  EXPECT_EQ(model->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest,
       SuccessCallbackUnaffectedByReentrantCancelOnStepReset) {
  CoreAccountInfo account = SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  ReentrantCancelOnResetObserver reentrant_observer(model);
  EXPECT_CALL(mock_pin_service(), SetPin(Eq(account), _, _))
      .WillOnce(RunOnceCallback<2>(true));

  model->OnGPMPinEntered(u"654321");
  EXPECT_TRUE(change_future.Get());
  EXPECT_EQ(model->step(), Step::kNotStarted);
}

TEST_F(LocalChangePinControllerImplTest,
       SecondStartChangePinRejectedWhileFlowActive) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_callback;
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(MoveArg<1>(&auth_callback));
  EXPECT_CALL(*authenticator, Cancel());

  base::test::TestFuture<bool> first_future;
  controller()->StartChangePin(first_future.GetCallback());
  EXPECT_FALSE(first_future.IsReady());

  base::test::TestFuture<bool> second_future;
  controller()->StartChangePin(second_future.GetCallback());
  EXPECT_FALSE(second_future.Get());
  EXPECT_FALSE(first_future.IsReady());

  controller()->model_for_testing()->CancelAuthenticatorRequest();
  EXPECT_FALSE(first_future.Get());
}

// On ChromeOS Ash, a Profile's primary account is bound to the OS user session
// and cannot be cleared or switched mid-session.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(LocalChangePinControllerImplTest,
       AccountClearedBeforeOsUvCompletesShowsError) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_callback;
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(MoveArg<1>(&auth_callback));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  ASSERT_TRUE(auth_callback);

  ClearPrimaryAccount();
  std::move(auth_callback).Run(true);

  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  EXPECT_EQ(model->step(), Step::kGPMError);
  EXPECT_FALSE(change_future.IsReady());

  model->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       AccountSwitchedBeforePinOptionChangedShowsError) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  ClearPrimaryAccount();
  SignInPrimaryAccount(kOtherEmail);

  model->OnGPMPinOptionChanged(true);
  EXPECT_EQ(model->step(), Step::kGPMError);
  EXPECT_FALSE(change_future.IsReady());

  model->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
}

TEST_F(LocalChangePinControllerImplTest,
       AccountClearedBeforePinEnteredShowsError) {
  SignInPrimaryAccount();
  auto* authenticator = ConfigureOsUv(/*available=*/true);
  EXPECT_CALL(*authenticator, AuthenticateWithMessage(_, _))
      .WillOnce(RunOnceCallback<1>(true));

  base::test::TestFuture<bool> change_future;
  controller()->StartChangePin(change_future.GetCallback());
  AuthenticatorRequestDialogModel* model = controller()->model_for_testing();
  ASSERT_EQ(model->step(), Step::kGPMChangePin);

  ClearPrimaryAccount();

  model->OnGPMPinEntered(u"654321");
  EXPECT_EQ(model->step(), Step::kGPMError);
  EXPECT_FALSE(change_future.IsReady());

  model->CancelAuthenticatorRequest();
  EXPECT_FALSE(change_future.Get());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace webauthn
