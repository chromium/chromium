// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROJECTOR_APP_TEST_MOCK_APP_CLIENT_H_
#define CHROMEOS_COMPONENTS_PROJECTOR_APP_TEST_MOCK_APP_CLIENT_H_

#include <string>

#include "base/time/time.h"
#include "chromeos/components/projector_app/projector_app_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {

class MockAppClient : public ProjectorAppClient {
 public:
  MockAppClient();
  MockAppClient(const MockAppClient&) = delete;
  MockAppClient& operator=(const MockAppClient&) = delete;
  ~MockAppClient() override;

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  // ProjectorAppClient:
  signin::IdentityManager* GetIdentityManager() override;
  network::mojom::URLLoaderFactory* GetUrlLoaderFactory() override;
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(OnNewScreencastPreconditionChanged, void(bool));

  void SetAutomaticIssueOfAccessTokens(bool success);
  void WaitForAccessRequest(const std::string& account_email);
  void GrantOAuthTokenFor(const std::string& account_email,
                          const base::Time& expiry_time);
  void AddSecondaryAccount(const std::string& account_email);

 private:
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PROJECTOR_APP_TEST_MOCK_APP_CLIENT_H_
