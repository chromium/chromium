// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/cable_registration_state.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "device/fido/cable/v2_handshake.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

using device::cablev2::authenticator::Registration;

namespace webauthn::authenticator {
namespace {

class TestRegistration : public Registration {
 public:
  TestRegistration(
      base::OnceCallback<void()> on_ready,
      base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
          event_callback)
      : on_ready_(std::move(on_ready)), on_event_(std::move(event_callback)) {}

  ~TestRegistration() override = default;

  void PrepareContactID() override {}
  void RotateContactID() override {}
  std::optional<std::vector<uint8_t>> contact_id() const override {
    return contact_id_;
  }

  static constexpr std::vector<uint8_t> ContactID() { return {1, 2, 3, 4}; }

  base::OnceCallback<void()> on_ready_;
  const base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
      on_event_;
  std::optional<std::vector<uint8_t>> contact_id_ = ContactID();
};

class TestSystemInterface : public RegistrationState::SystemInterface {
 public:
  ~TestSystemInterface() override = default;

  std::unique_ptr<Registration> NewRegistration(
      Registration::Type type,
      base::OnceCallback<void()> on_ready,
      base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
          event_callback) override {
    auto registration = std::make_unique<TestRegistration>(
        std::move(on_ready), std::move(event_callback));
    if (type == Registration::Type::LINKING) {
      linking_registration_ = registration.get();
    } else {
      syncing_registration_ = registration.get();
    }
    return registration;
  }

  std::string GetRootSecret() override { return root_secret_; }

  void SetRootSecret(std::string secret) override {
    root_secret_ = std::move(secret);
  }

  void CanDeviceSupportCable(base::OnceCallback<void(bool)> callback) override {
    CHECK(!support_callback_);
    support_callback_ = std::move(callback);
  }

  void AmInWorkProfile(base::OnceCallback<void(bool)> callback) override {
    CHECK(!work_profile_callback_);
    work_profile_callback_ = std::move(callback);
  }

  void CalculateIdentityKey(
      const std::array<uint8_t, 32>& secret,
      base::OnceCallback<void(bssl::UniquePtr<EC_KEY>)> callback) override {
    CHECK(!identity_key_callback_);
    identity_key_callback_ = std::move(callback);
  }

  void OnCloudMessage(std::vector<uint8_t> serialized,
                      bool is_make_credential) override {
    on_cloud_message_called_ = true;
  }

  void RefreshLocalDeviceInfo() override {
    refresh_local_device_info_called_ = true;
  }

  void GetPrelinkFromPlayServices(
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
      override {
    CHECK(!prelink_callback_);
    prelink_callback_ = std::move(callback);
  }

  std::string root_secret_;
  raw_ptr<TestRegistration> linking_registration_ = nullptr;
  raw_ptr<TestRegistration> syncing_registration_ = nullptr;
  bool on_cloud_message_called_ = false;
  bool refresh_local_device_info_called_ = false;

  base::OnceCallback<void(bool)> support_callback_;
  base::OnceCallback<void(bool)> work_profile_callback_;
  base::OnceCallback<void(bssl::UniquePtr<EC_KEY>)> identity_key_callback_;
  base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
      prelink_callback_;
};

class CableRegistrationStateTest : public testing::Test {
  void SetUp() override {
    auto interface = std::make_unique<TestSystemInterface>();
    interface_ = interface.get();
    state_ = std::make_unique<RegistrationState>(std::move(interface));
  }

 protected:
  static bssl::UniquePtr<EC_KEY> FakeKey() {
    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    return bssl::UniquePtr<EC_KEY>(
        EC_KEY_derive_from_secret(p256.get(), nullptr, 0));
  }

  raw_ptr<TestSystemInterface> interface_ = nullptr;
  std::unique_ptr<RegistrationState> state_;
};

constexpr unsigned kLengthOf32BytesBase64Encoded = 44;

TEST_F(CableRegistrationStateTest, EmptySecret) {
  state_->Register();
  // 44 is the length of a 32-byte secret, base64-encoded.
  EXPECT_EQ(interface_->root_secret_.size(), kLengthOf32BytesBase64Encoded);
}

TEST_F(CableRegistrationStateTest, WrongLengthSecret) {
  interface_->root_secret_ = "AAAA";
  state_->Register();
  EXPECT_EQ(interface_->root_secret_.size(), kLengthOf32BytesBase64Encoded);
}

TEST_F(CableRegistrationStateTest, SecretMaintained) {
  const std::string secret = "zs0gi/qLipq53eg24sccdaPKcpSEgSwE0Jd9kZLj4DU=";
  interface_->root_secret_ = secret;
  state_->Register();
  EXPECT_EQ(interface_->root_secret_, secret);
}

TEST_F(CableRegistrationStateTest, HaveDataForSync) {
  state_->Register();
  EXPECT_FALSE(state_->have_data_for_sync());
  std::move(interface_->support_callback_).Run(true);
  EXPECT_FALSE(state_->have_data_for_sync());
  std::move(interface_->identity_key_callback_).Run(FakeKey());
  EXPECT_FALSE(state_->have_data_for_sync());
  EXPECT_FALSE(interface_->refresh_local_device_info_called_);
  state_->SignalSyncWhenReady();
  EXPECT_FALSE(state_->have_data_for_sync());
  state_->SignalSyncWhenReady();
  EXPECT_FALSE(state_->have_data_for_sync());
  std::move(interface_->prelink_callback_).Run(std::nullopt);
  EXPECT_FALSE(interface_->refresh_local_device_info_called_);
  EXPECT_FALSE(state_->have_data_for_sync());
  std::move(interface_->work_profile_callback_).Run(true);
  EXPECT_TRUE(interface_->refresh_local_device_info_called_);
  EXPECT_TRUE(state_->have_data_for_sync());
  EXPECT_TRUE(state_->device_supports_cable());
  EXPECT_TRUE(state_->am_in_work_profile());
  EXPECT_FALSE(state_->link_data_from_play_services().has_value());
}

TEST_F(CableRegistrationStateTest,
       DontProcessLinkingEventsBeforeContactIdReady) {
  state_->Register();
  EXPECT_FALSE(interface_->on_cloud_message_called_);

  interface_->linking_registration_->contact_id_ = std::nullopt;
  auto event = std::make_unique<Registration::Event>();
  event->source = Registration::Type::LINKING;
  interface_->linking_registration_->on_event_.Run(std::move(event));
  EXPECT_FALSE(interface_->on_cloud_message_called_);

  interface_->linking_registration_->contact_id_ =
      TestRegistration::ContactID();
  std::move(interface_->linking_registration_->on_ready_).Run();
  EXPECT_TRUE(interface_->on_cloud_message_called_);
}

TEST_F(CableRegistrationStateTest, SyncEventTooOld) {
  state_->Register();
  EXPECT_FALSE(interface_->on_cloud_message_called_);

  for (const bool too_old : {false, true}) {
    auto event = std::make_unique<Registration::Event>();
    event->source = Registration::Type::SYNC;
    const uint64_t now = device::cablev2::sync::IDNow() - (too_old ? 50 : 0);
    memcpy(event->pairing_id.data(), &now, event->pairing_id.size());

    interface_->on_cloud_message_called_ = false;
    interface_->syncing_registration_->on_event_.Run(std::move(event));
    EXPECT_EQ(interface_->on_cloud_message_called_, !too_old);
  }
}

}  // namespace

}  // namespace webauthn::authenticator
