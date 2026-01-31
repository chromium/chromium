// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/ui_readiness_barrier.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/password_credential_fetcher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_renderer_host.h"
#include "device/fido/fido_request_handler_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

class MockUiReadinessBarrierDelegate : public UiReadinessBarrier::Delegate {
 public:
  MockUiReadinessBarrierDelegate() = default;
  ~MockUiReadinessBarrierDelegate() = default;

  MOCK_METHOD(void,
              ShowUI,
              (device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
               PasswordCredentialFetcher::PasswordCredentials passwords),
              (override));
  MOCK_METHOD(bool, PasswordsUsable, (), (override));
  MOCK_METHOD(bool, IsEnclaveActive, (), (override));
  MOCK_METHOD(bool, IsEnclaveReady, (), (override));
  MOCK_METHOD(void,
              GetGpmPasskeys,
              (device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
               base::OnceCallback<void(
                   device::FidoRequestHandlerBase::TransportAvailabilityInfo)>
                   callback),
              (override));
};

class UiReadinessBarrierTest : public ChromeRenderViewHostTestHarness {
 public:
  UiReadinessBarrierTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    model_ = base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
    barrier_ = std::make_unique<UiReadinessBarrier>(&delegate_, model_.get());
  }

  void TearDown() override {
    barrier_.reset();
    model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  device::FidoRequestHandlerBase::TransportAvailabilityInfo MakeTAI() {
    device::FidoRequestHandlerBase::TransportAvailabilityInfo tai;
    return tai;
  }

  PasswordCredentialFetcher::PasswordCredentials MakePasswords() {
    PasswordCredentialFetcher::PasswordCredentials passwords;
    return passwords;
  }

  MockUiReadinessBarrierDelegate delegate_;
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<UiReadinessBarrier> barrier_;
};

TEST_F(UiReadinessBarrierTest, BasicFlow_AllReady) {
  // Setup delegate to indicate everything is ready.
  EXPECT_CALL(delegate_, PasswordsUsable()).WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, IsEnclaveActive()).WillRepeatedly(Return(false));
  EXPECT_CALL(delegate_, IsEnclaveReady()).WillRepeatedly(Return(true));

  // Expect ShowUI to be called once both inputs are provided.
  EXPECT_CALL(delegate_, ShowUI(_, _)).Times(1);

  barrier_->SetTransportAvailabilityInfo(MakeTAI());
  barrier_->SetPasswordCredentials(MakePasswords());
}

TEST_F(UiReadinessBarrierTest, EnclaveNotReady_DelaysUI) {
  EXPECT_CALL(delegate_, PasswordsUsable()).WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, IsEnclaveActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, IsEnclaveReady()).WillRepeatedly(Return(false));

  // Should NOT show UI yet.
  EXPECT_CALL(delegate_, ShowUI(_, _)).Times(0);

  barrier_->SetTransportAvailabilityInfo(MakeTAI());
  barrier_->SetPasswordCredentials(MakePasswords());

  // Now make enclave ready.
  EXPECT_CALL(delegate_, IsEnclaveReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, ShowUI(_, _)).Times(1);
  // Simulating GPM passkey fetch
  EXPECT_CALL(delegate_, GetGpmPasskeys(_, _))
      .WillOnce(
          [](device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
             base::OnceCallback<void(
                 device::FidoRequestHandlerBase::TransportAvailabilityInfo)>
                 callback) { std::move(callback).Run(std::move(tai)); });

  model_->OnGPMReadyForUI();
}
