// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_web_ui.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/webauthn/local_credential_management.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace settings {

namespace {

constexpr size_t kBioEnrollCapacity = 3;
constexpr char kTestPIN[] = "1234";
constexpr uint8_t kCredentialID[] = {0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
                                     0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa};
constexpr char kRPID[] = "example.com";
constexpr char kRPName[] = "Example Corp";
constexpr uint8_t kUserID[] = {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                               0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1};
constexpr char kUserName[] = "alice@example.com";
constexpr char kUserDisplayName[] = "Alice Example <alice@example.com>";

class TestSecurityKeysCredentialHandler : public SecurityKeysCredentialHandler {
 public:
  explicit TestSecurityKeysCredentialHandler(content::TestWebUI* web_ui)
      : SecurityKeysCredentialHandler(
            std::make_unique<device::test::VirtualFidoDeviceFactory>()) {
    set_web_ui(web_ui);
    AllowJavascriptForTesting();
  }

  using SecurityKeysCredentialHandler::HandlePIN;
  using SecurityKeysCredentialHandler::HandleStart;
  using SecurityKeysCredentialHandler::HandleUpdateUserInformation;

  device::test::VirtualFidoDeviceFactory* GetDiscoveryFactory() {
    return static_cast<device::test::VirtualFidoDeviceFactory*>(
        discovery_factory());
  }

  // Simulates a call to HandleStart() and returns the ID of the completed
  // callback.
  std::string SimulateStart() {
    constexpr char kCallbackId[] = "securityKeyCredentialManagementStart";
    base::Value::List args;
    args.Append(kCallbackId);
    HandleStart(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
  }

  // Simulates a call to HandleProvidePIN() and returns the ID of the completed
  // callback.
  std::string SimulateProvidePIN() {
    constexpr char kCallbackId[] = "securityKeyCredentialManagementPIN";
    base::Value::List args;
    args.Append(kCallbackId);
    args.Append(kTestPIN);
    HandlePIN(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
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

  // Simulates a call to HandleStart() and returns the ID of the completed
  // callback.
  std::string SimulateStart() {
    constexpr char kCallbackId[] = "bioEnrollStart";
    base::Value::List args;
    args.Append(kCallbackId);
    HandleStart(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
  }

  // Simulates a call to HandleProvidePIN() and returns the ID of the completed
  // callback.
  std::string SimulateProvidePIN() {
    constexpr char kCallbackId[] = "bioEnrollProvidePIN";
    base::Value::List args;
    args.Append(kCallbackId);
    args.Append(kTestPIN);
    HandleProvidePIN(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
  }

  // Simulates a call to HandleStartEnrolling() and returns the ID of the
  // completed callback.
  std::string SimulateStartEnrolling() {
    constexpr char kCallbackId[] = "bioEnrollStartEnrolling";
    base::Value::List args;
    args.Append(kCallbackId);
    HandleStartEnrolling(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
  }
};

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

TEST_F(SecurityKeysCredentialHandlerTest,
       TestUpdateUserInformationNotSupported) {
  handler_->GetDiscoveryFactory()->mutable_state()->pin = kTestPIN;
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.credential_management_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_0};
  handler_->GetDiscoveryFactory()->SetCtap2Config(config);

  std::string start_callback_id = handler_->SimulateStart();
  ASSERT_EQ(web_ui_->call_data()[0]->arg1()->GetString(), start_callback_id);
  ASSERT_TRUE(web_ui_->call_data()[0]->arg3()->is_dict());
  const base::Value::Dict& response =
      web_ui_->call_data()[0]->arg3()->GetDict();
  EXPECT_FALSE(*response.FindBool("supportsUpdateUserInformation"));
}

TEST_F(SecurityKeysCredentialHandlerTest, TestUpdateUserInformation) {
  handler_->GetDiscoveryFactory()->mutable_state()->pin = kTestPIN;
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.min_pin_length_support = true;
  config.credential_management_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  handler_->GetDiscoveryFactory()->SetCtap2Config(config);

  std::vector<uint8_t> credential_id =
      device::fido_parsing_utils::Materialize(kCredentialID);

  device::PublicKeyCredentialRpEntity rp(kRPID, kRPName);
  device::PublicKeyCredentialUserEntity user(
      device::fido_parsing_utils::Materialize(kUserID), kUserName,
      kUserDisplayName);

  ASSERT_TRUE(
      handler_->GetDiscoveryFactory()->mutable_state()->InjectResidentKey(
          kCredentialID, rp, user));

  EXPECT_EQ(handler_->GetDiscoveryFactory()
                ->mutable_state()
                ->registrations[credential_id]
                .user,
            user);

  std::string credential_id_hex = base::HexEncode(credential_id);
  std::string user_id_hex = base::HexEncode(kUserID);
  std::string new_username = "jsapple@example.com";
  std::string new_displayname = "John S. Apple";

  base::Value::List args;
  args.Append("securityKeyCredentialManagementUpdate");
  args.Append(credential_id_hex);
  args.Append(user_id_hex);
  args.Append(new_username);
  args.Append(new_displayname);

  std::string start_callback_id = handler_->SimulateStart();
  ASSERT_EQ(web_ui_->call_data()[0]->arg1()->GetString(), start_callback_id);
  ASSERT_TRUE(web_ui_->call_data()[0]->arg3()->is_dict());
  const base::Value::Dict& response =
      web_ui_->call_data()[0]->arg3()->GetDict();
  EXPECT_TRUE(*response.FindBool("supportsUpdateUserInformation"));

  handler_->SimulateProvidePIN();
  handler_->HandleUpdateUserInformation(args);
  base::RunLoop().RunUntilIdle();

  device::PublicKeyCredentialUserEntity updated_user(
      device::fido_parsing_utils::Materialize(kUserID), new_username,
      new_displayname);

  EXPECT_EQ(handler_->GetDiscoveryFactory()
                ->mutable_state()
                ->registrations[credential_id]
                .user,
            updated_user);
}

TEST_F(SecurityKeysCredentialHandlerTest, TestForcePINChange) {
  handler_->GetDiscoveryFactory()->mutable_state()->force_pin_change = true;
  handler_->GetDiscoveryFactory()->mutable_state()->pin = kTestPIN;
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.min_pin_length_support = true;
  config.credential_management_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  handler_->GetDiscoveryFactory()->SetCtap2Config(config);

  std::string callback_id("start_callback_id");
  base::Value::List args;
  args.Append(callback_id);
  handler_->HandleStart(args);
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

  // Sets up the VirtualFidoDeviceFactory with a security key that supports bio
  // enrollment.
  void ConfigureVirtualAuthenticator() {
    handler_->GetDiscoveryFactory()->mutable_state()->pin = kTestPIN;
    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.bio_enrollment_support = true;
    config.bio_enrollment_capacity = kBioEnrollCapacity;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.min_pin_length_support = true;
    config.credential_management_support = true;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    handler_->GetDiscoveryFactory()->SetCtap2Config(config);
  }

  std::unique_ptr<TestSecurityKeysBioEnrollmentHandler> handler_;
  std::unique_ptr<content::TestWebUI> web_ui_;
};

TEST_F(SecurityKeysBioEnrollmentHandlerTest, TestForcePINChange) {
  ConfigureVirtualAuthenticator();
  handler_->GetDiscoveryFactory()->mutable_state()->force_pin_change = true;

  const std::string callback_id = handler_->SimulateStart();

  EXPECT_EQ(web_ui_->call_data()[0]->arg1()->GetString(),
            "security-keys-bio-enroll-error");
  EXPECT_EQ(
      web_ui_->call_data()[0]->arg2()->GetString(),
      l10n_util::GetStringUTF8(IDS_SETTINGS_SECURITY_KEYS_FORCE_PIN_CHANGE));
  EXPECT_EQ(web_ui_->call_data()[0]->arg3()->GetBool(), true);
}

TEST_F(SecurityKeysBioEnrollmentHandlerTest, TestStorageFullError) {
  ConfigureVirtualAuthenticator();
  // Insert fingerprint templates to exhaust the key's storage.
  handler_->GetDiscoveryFactory()->mutable_state()->bio_templates = {
      {0, "finger0"},
      {1, "finger1"},
      {2, "finger2"},
  };
  ASSERT_EQ(
      handler_->GetDiscoveryFactory()->mutable_state()->bio_templates.size(),
      kBioEnrollCapacity);

  handler_->SimulateStart();
  handler_->SimulateProvidePIN();
  const std::string callback_id = handler_->SimulateStartEnrolling();

  EXPECT_EQ(web_ui_->call_data().size(), 3u);
  EXPECT_EQ(web_ui_->call_data()[2]->arg1()->GetString(), callback_id);
  EXPECT_EQ(web_ui_->call_data()[2]->arg2()->GetBool(), true);
  EXPECT_TRUE(web_ui_->call_data()[2]->arg3()->is_dict());
  base::Value::Dict expected;
  expected.Set("code",
               static_cast<int>(
                   device::CtapDeviceResponseCode::kCtap2ErrFpDatabaseFull));
  expected.Set("remaining", 0);
  EXPECT_EQ(*web_ui_->call_data()[2]->arg3(), expected);
}

}  // namespace

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
class MockLocalCredentialManagement : public LocalCredentialManagement {
 public:
  ~MockLocalCredentialManagement() override = default;

  MOCK_METHOD(void, HasCredentials, (base::OnceCallback<void(bool)> callback));
  MOCK_METHOD(
      void,
      Enumerate,
      (base::OnceCallback<void(
           std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
           callback));
  MOCK_METHOD(void,
              Delete,
              (base::span<const uint8_t> credential_id,
               base::OnceCallback<void(bool)> callback));
  MOCK_METHOD(void,
              Edit,
              (base::span<uint8_t> credential_id,
               std::string new_username,
               base::OnceCallback<void(bool)> callback));
};

class TestPasskeysHandler : public PasskeysHandler {
 public:
  TestPasskeysHandler(content::TestWebUI* web_ui,
                      std::unique_ptr<LocalCredentialManagement> lcm)
      : PasskeysHandler(std::move(lcm)) {
    set_web_ui(web_ui);
    AllowJavascriptForTesting();
  }

  using PasskeysHandler::HandleDelete;
  using PasskeysHandler::HandleEdit;

  std::string SimulateEdit(std::string credential_id,
                           std::string new_username) {
    constexpr char kCallbackId[] = "passkeysEdit";
    base::Value::List args;
    args.Append(kCallbackId);
    args.Append(credential_id);
    args.Append(new_username);
    HandleEdit(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
  }

  std::string SimulateDelete(std::string credential_id) {
    constexpr char kCallbackId[] = "passkeysDelete";
    base::Value::List args;
    args.Append(kCallbackId);
    args.Append(credential_id);
    HandleDelete(args);
    base::RunLoop().RunUntilIdle();
    return kCallbackId;
  }
};

class PasskeysHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    local_cred_man_ = std::make_unique<MockLocalCredentialManagement>();
    weak_local_cred_man_ = local_cred_man_.get();
    handler_ = std::make_unique<TestPasskeysHandler>(
        web_ui_.get(), std::move(local_cred_man_));
    web_ui_->ClearTrackedCalls();
  }

  std::unique_ptr<TestPasskeysHandler> handler_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<MockLocalCredentialManagement> local_cred_man_;
  raw_ptr<MockLocalCredentialManagement> weak_local_cred_man_;
};

TEST_F(PasskeysHandlerTest, TestHandleEdit) {
  std::vector<uint8_t> credential_id =
      device::fido_parsing_utils::Materialize(kCredentialID);
  std::string credential_id_hex = base::HexEncode(credential_id);
  EXPECT_CALL(
      *weak_local_cred_man_,
      Edit(testing::ElementsAreArray(kCredentialID),
           testing::Eq("new-username"), base::test::IsNotNullCallback()))
      .Times(testing::AtLeast(1))
      .WillOnce([](base::span<uint8_t> credential_id, std::string new_username,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
        base::RunLoop().RunUntilIdle();
      });
  EXPECT_CALL(*weak_local_cred_man_, Enumerate)
      .WillOnce([](base::OnceCallback<
                    void(std::optional<
                         std::vector<device::DiscoverableCredentialMetadata>>)>
                       callback) {
        std::vector<device::DiscoverableCredentialMetadata> credential_metadata{
            device::DiscoverableCredentialMetadata{
                device::AuthenticatorType::kOther,
                "a.com",
                {0},
                device::PublicKeyCredentialUserEntity(
                    {0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
                     0xa, 0xa, 0xa, 0xa},
                    "new-username", "new-username")}};
        std::move(callback).Run(std::move(credential_metadata));
        base::RunLoop().RunUntilIdle();
      });
  handler_->SimulateEdit(credential_id_hex, "new-username");
  EXPECT_EQ(web_ui_->call_data()[0]->arg1()->GetString(), "passkeysEdit");
  EXPECT_EQ(web_ui_->call_data()[0]->arg2()->GetBool(), true);
  EXPECT_EQ(*web_ui_->call_data()[0]->arg3()->GetList()[0].GetDict().FindString(
                "userName"),
            "new-username");
}

TEST_F(PasskeysHandlerTest, TestRecordPasskeyDelete) {
  std::vector<uint8_t> credential_id =
      device::fido_parsing_utils::Materialize(kCredentialID);
  std::string credential_id_hex = base::HexEncode(credential_id);
  EXPECT_CALL(*weak_local_cred_man_,
              Delete(testing::ElementsAreArray(kCredentialID),
                     base::test::IsNotNullCallback()))
      .Times(testing::AtLeast(1))
      .WillOnce([](auto response, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
        base::RunLoop().RunUntilIdle();
      });
  EXPECT_CALL(*weak_local_cred_man_, Enumerate)
      .WillOnce(
          [](base::OnceCallback<void(
                 std::optional<std::vector<
                     device::DiscoverableCredentialMetadata>>)> callback) {
            std::move(callback).Run(/*credential_metadata=*/{});
            base::RunLoop().RunUntilIdle();
          });
  handler_->SimulateDelete(credential_id_hex);
  EXPECT_EQ(web_ui_->call_data()[0]->arg1()->GetString(), "passkeysDelete");
  EXPECT_EQ(web_ui_->call_data()[0]->arg2()->GetBool(), true);
}

#endif
}  // namespace settings
