// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/credential_storage.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class CredentialStorageTest : public testing::Test {
 public:
  CredentialStorageTest() = default;

  ~CredentialStorageTest() override = default;

  // testing::Test:
  void SetUp() override {
    credential_storage_ =
        std::make_unique<CredentialStorage>(remote_credential_storage);
  }

 protected:
  mojo::SharedRemote<
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      remote_credential_storage;
  std::unique_ptr<CredentialStorage> credential_storage_;
};

TEST_F(CredentialStorageTest, Initialize) {
  EXPECT_TRUE(credential_storage_);
}

}  // namespace nearby::chrome
