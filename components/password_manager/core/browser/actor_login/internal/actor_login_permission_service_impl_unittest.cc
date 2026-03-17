// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {

namespace {
const char kTestListUrl[] =
    "https://staging-agenticpermission.pa.sandbox.googleapis.com/v1/"
    "permissions:list";
const char kTestDeleteUrl[] =
    "https://staging-agenticpermission.pa.sandbox.googleapis.com/v1/"
    "permissions:delete";
}  // namespace

class ActorLoginPermissionServiceImplTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  ActorLoginPermissionServiceImpl service_{
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_)};
};

TEST_F(ActorLoginPermissionServiceImplTest, ListAllPermissionsReturnsEmpty) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestListUrl,
                                                             "{}");

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsReturnsPermissions) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());

  const char kValidResponse[] = R"({
    "permissions": [
      {
        "federatedCredentialPermission": {
          "idpOrigin": "https://idp.com",
          "rpEmbedderOrigin": "https://embedder.com",
          "rpRequesterOrigin": "https://requester.com",
          "chosenAccountId": "account123",
          "chosenAccountEmail": "user@idp.com",
          "affiliatedRequesterOrigins": [
            "https://aff1.com",
            "https://aff2.com"
          ]
        }
      },
      {
        "federatedCredentialPermission": {
          "idpOrigin": "https://idp2.com"
        }
      },
      {
        "otherType": {}
      }
    ]
  })";

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestListUrl,
                                                             kValidResponse);

  std::vector<FederatedPermission> permissions = future.Get();
  ASSERT_EQ(2u, permissions.size());

  EXPECT_EQ(url::Origin::Create(GURL("https://idp.com")),
            permissions[0].idp_origin);
  EXPECT_EQ(url::Origin::Create(GURL("https://embedder.com")),
            permissions[0].rp_embedder_origin);
  EXPECT_EQ(url::Origin::Create(GURL("https://requester.com")),
            permissions[0].rp_requester_origin);
  EXPECT_EQ("account123", permissions[0].chosen_account_id);
  EXPECT_EQ("user@idp.com", permissions[0].chosen_account_email);
  EXPECT_EQ(std::vector<std::string>({"https://aff1.com", "https://aff2.com"}),
            permissions[0].affiliated_requester_origins);

  EXPECT_EQ(url::Origin::Create(GURL("https://idp2.com")),
            permissions[1].idp_origin);
  EXPECT_TRUE(permissions[1].rp_embedder_origin.opaque());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsHandlesNetworkError) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestListUrl, "", net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsHandlesInvalidJson) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestListUrl,
                                                             "{ invalid json");

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsHandlesMultipleRequests) {
  base::test::TestFuture<std::vector<FederatedPermission>> future1;
  base::test::TestFuture<std::vector<FederatedPermission>> future2;

  service_.ListAllPermissions(future1.GetCallback());
  service_.ListAllPermissions(future2.GetCallback());

  EXPECT_EQ(2, test_url_loader_factory_.NumPending());

  // Resolve the first request.
  const char kResponse1[] = R"({
    "permissions": [
      {
        "federatedCredentialPermission": {
          "idpOrigin": "https://idp1.com"
        }
      }
    ]
  })";
  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestListUrl,
                                                             kResponse1);

  // Resolve the second request.
  const char kResponse2[] = R"({
    "permissions": [
      {
        "federatedCredentialPermission": {
          "idpOrigin": "https://idp2.com"
        }
      },
      {
        "federatedCredentialPermission": {
          "idpOrigin": "https://idp3.com"
        }
      }
    ]
  })";
  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestListUrl,
                                                             kResponse2);

  std::vector<FederatedPermission> permissions1 = future1.Get();
  ASSERT_EQ(1u, permissions1.size());
  EXPECT_EQ(url::Origin::Create(GURL("https://idp1.com")),
            permissions1[0].idp_origin);

  std::vector<FederatedPermission> permissions2 = future2.Get();
  ASSERT_EQ(2u, permissions2.size());
  EXPECT_EQ(url::Origin::Create(GURL("https://idp2.com")),
            permissions2[0].idp_origin);
  EXPECT_EQ(url::Origin::Create(GURL("https://idp3.com")),
            permissions2[1].idp_origin);
}

TEST_F(ActorLoginPermissionServiceImplTest,
       DeletePermissionSendsCorrectRequest) {
  base::test::TestFuture<bool> future;
  service_.DeletePermission(url::Origin::Create(GURL("https://embedder.com")),
                            future.GetCallback());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(kTestDeleteUrl, request.url.spec());
  EXPECT_EQ("POST", request.method);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
  EXPECT_EQ(base::test::ParseJson(R"({
              "filter": [
                {
                  "federatedCredentialPermissionFilter": {
                    "matchAffiliatedRequesterOrigins": true,
                    "rpEmbedderOrigin": "https://embedder.com"
                  }
                }
              ]
            })"),
            base::test::ParseJson(network::GetUploadData(request)));
}

TEST_F(ActorLoginPermissionServiceImplTest,
       DeletePermissionReturnsTrueOnSuccess) {
  base::test::TestFuture<bool> future;
  service_.DeletePermission(url::Origin::Create(GURL("https://embedder.com")),
                            future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestDeleteUrl,
                                                             "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       DeletePermissionReturnsFalseOnError) {
  base::test::TestFuture<bool> future;
  service_.DeletePermission(url::Origin::Create(GURL("https://embedder.com")),
                            future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestDeleteUrl, "", net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_FALSE(future.Get());
}

}  // namespace actor_login
