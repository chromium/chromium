// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/secure_channel_disconnector_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/fake_connection.h"
#include "chromeos/services/secure_channel/fake_secure_channel_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelSecureChannelDisconnectorImplTest : public testing::Test {
 protected:
  SecureChannelSecureChannelDisconnectorImplTest() = default;
  ~SecureChannelSecureChannelDisconnectorImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    disconnector_ =
        SecureChannelDisconnectorImpl::Factory::Get()->BuildInstance();
  }

  // Returns an ID associated with the request as well as a pointer to the
  // SecureChannel to be disconnected.
  std::pair<base::UnguessableToken, FakeSecureChannelConnection*>
  CallDisconnectSecureChannel() {
    auto fake_secure_channel = std::make_unique<FakeSecureChannelConnection>(
        std::make_unique<FakeConnection>(
            multidevice::CreateRemoteDeviceRefForTest()));
    fake_secure_channel->ChangeStatus(SecureChannel::Status::CONNECTED);

    FakeSecureChannelConnection* fake_secure_channel_raw =
        fake_secure_channel.get();
    base::UnguessableToken id = base::UnguessableToken::Create();

    fake_secure_channel->set_destructor_callback(base::BindOnce(
        &SecureChannelSecureChannelDisconnectorImplTest::OnSecureChannelDeleted,
        base::Unretained(this), id));

    disconnector_->DisconnectSecureChannel(std::move(fake_secure_channel));

    return std::make_pair(id, fake_secure_channel_raw);
  }

  bool HasChannelBeenDeleted(const base::UnguessableToken id) {
    return base::Contains(deleted_request_ids_, id);
  }

 private:
  void OnSecureChannelDeleted(const base::UnguessableToken& id) {
    deleted_request_ids_.insert(id);
  }

  base::flat_set<base::UnguessableToken> deleted_request_ids_;

  std::unique_ptr<SecureChannelDisconnector> disconnector_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelSecureChannelDisconnectorImplTest);
};

TEST_F(SecureChannelSecureChannelDisconnectorImplTest,
       TestDoesNotDeleteUntilDisconnected) {
  // Call disconnect. The channel should not have yet been deleted.
  auto id_and_channel_pair_1 = CallDisconnectSecureChannel();
  EXPECT_FALSE(HasChannelBeenDeleted(id_and_channel_pair_1.first));

  // Call disconnect on a second channel; neither should have been deleted.
  auto id_and_channel_pair_2 = CallDisconnectSecureChannel();
  EXPECT_FALSE(HasChannelBeenDeleted(id_and_channel_pair_2.first));

  // Update to disconnecting. This should not cause the channel to be deleted.
  id_and_channel_pair_1.second->ChangeStatus(
      SecureChannel::Status::DISCONNECTING);
  EXPECT_FALSE(HasChannelBeenDeleted(id_and_channel_pair_1.first));
  id_and_channel_pair_2.second->ChangeStatus(
      SecureChannel::Status::DISCONNECTING);
  EXPECT_FALSE(HasChannelBeenDeleted(id_and_channel_pair_2.first));

  // Update to disconnected. The channels should be deleted.
  id_and_channel_pair_1.second->ChangeStatus(
      SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(HasChannelBeenDeleted(id_and_channel_pair_1.first));
  id_and_channel_pair_2.second->ChangeStatus(
      SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(HasChannelBeenDeleted(id_and_channel_pair_2.first));
}

}  // namespace secure_channel

}  // namespace chromeos
