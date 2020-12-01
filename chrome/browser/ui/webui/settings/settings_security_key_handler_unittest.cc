// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"

#include <memory>

#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_web_ui.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_types.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

namespace {

class TestSecurityKeysCredentialHandler : public SecurityKeysCredentialHandler {
 public:
  explicit TestSecurityKeysCredentialHandler(content::TestWebUI* web_ui)
      : SecurityKeysCredentialHandler(
            std::make_unique<device::test::VirtualFidoDeviceFactory>()) {
    set_web_ui(web_ui);
    AllowJavascriptForTesting();
  }

  using SecurityKeysCredentialHandler::HandleStart;

  device::test::VirtualFidoDeviceFactory* GetDiscoveryFactory() {
    return static_cast<device::test::VirtualFidoDeviceFactory*>(
        discovery_factory());
  }
};

class TestSecurityKeysBioEnrollmentHandler
    : public SecurityKeysBioEnrollmentHandler {
 public:
  explicit TestSecurityKeysBioEnrollmentHandler(content::TestWebUI* web_ui)
      : SecurityKeysBioEnrollmentHandler(
            std::make_unique<device::test::VirtualFidoDeviceFactory>()) {
    set_web_ui(web_ui);
    AllowJavascriptForTesting();
  }

  using SecurityKeysBioEnrollmentHandler::HandleStart;

  device::test::VirtualFidoDeviceFactory* GetDiscoveryFactory() {
    return static_cast<device::test::VirtualFidoDeviceFactory*>(
        discovery_factory());
  }
};

}  // namespace

class SecurityKeysCredentialHandlerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    handler_ =
        std::make_unique<TestSecurityKeysCredentialHandler>(web_ui_.get());
    web_ui_->ClearTrackedCalls();
  }

  std::unique_ptr<TestSecurityKeysCredentialHandler> handler_;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

TEST_F(SecurityKeysCredentialHandlerTest, TestForcePINChange) {
  handler_->GetDiscoveryFactory()->mutable_state()->force_pin_change = true;
  handler_->GetDiscoveryFactory()->mutable_state()->pin = "1234";
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.min_pin_length_support = true;
  config.credential_management_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  handler_->GetDiscoveryFactory()->SetCtap2Config(config);

  std::string callback_id("start_callback_id");
  base::ListValue args;
  args.Append(callback_id);
  handler_->HandleStart(&args);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(web_ui_->call_data()[0]->arg1()->GetString(),
            "security-keys-credential-management-finished");
  EXPECT_EQ(
      web_ui_->call_data()[0]->arg2()->GetString(),
      l10n_util::GetStringUTF8(IDS_SETTINGS_SECURITY_KEYS_FORCE_PIN_CHANGE));
  EXPECT_EQ(web_ui_->call_data()[0]->arg3()->GetBool(), true);
}

class SecurityKeysBioEnrollmentHandlerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    handler_ =
        std::make_unique<TestSecurityKeysBioEnrollmentHandler>(web_ui_.get());
    web_ui_->ClearTrackedCalls();
  }

  std::unique_ptr<TestSecurityKeysBioEnrollmentHandler> handler_;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

TEST_F(SecurityKeysBioEnrollmentHandlerTest, TestForcePINChange) {
  handler_->GetDiscoveryFactory()->mutable_state()->force_pin_change = true;
  handler_->GetDiscoveryFactory()->mutable_state()->pin = "1234";
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.bio_enrollment_support = true;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.min_pin_length_support = true;
  config.credential_management_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  handler_->GetDiscoveryFactory()->SetCtap2Config(config);

  std::string callback_id("start_callback_id");
  base::ListValue args;
  args.Append(callback_id);
  handler_->HandleStart(&args);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(web_ui_->call_data()[0]->arg1()->GetString(),
            "security-keys-bio-enroll-error");
  EXPECT_EQ(
      web_ui_->call_data()[0]->arg2()->GetString(),
      l10n_util::GetStringUTF8(IDS_SETTINGS_SECURITY_KEYS_FORCE_PIN_CHANGE));
  EXPECT_EQ(web_ui_->call_data()[0]->arg3()->GetBool(), true);
}

}  // namespace settings
