// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"

namespace ash {

namespace device_sync {

MockCryptAuthClient::MockCryptAuthClient() {}

MockCryptAuthClient::~MockCryptAuthClient() {}

MockCryptAuthClientFactory::MockCryptAuthClientFactory(MockType mock_type)
    : mock_type_(mock_type) {}

MockCryptAuthClientFactory::~MockCryptAuthClientFactory() {}

std::unique_ptr<CryptAuthClient> MockCryptAuthClientFactory::CreateInstance() {
  std::unique_ptr<MockCryptAuthClient> client;
  if (mock_type_ == MockType::MAKE_STRICT_MOCKS)
    client = std::make_unique<testing::StrictMock<MockCryptAuthClient>>();
  else
    client = std::make_unique<testing::NiceMock<MockCryptAuthClient>>();

  for (auto& observer : observer_list_)
    observer.OnCryptAuthClientCreated(client.get());
  return client;
}

void MockCryptAuthClientFactory::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MockCryptAuthClientFactory::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace device_sync

}  // namespace ash
