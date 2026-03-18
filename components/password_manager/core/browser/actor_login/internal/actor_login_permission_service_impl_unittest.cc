// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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
const char kTestUpdateUrl[] =
    "https://staging-agenticpermission.pa.sandbox.googleapis.com/v1/"
    "permissions:update?allow_missing=true";

FederatedPermission CreateValidPermission() {
  FederatedPermission permission;
  permission.idp_origin = url::Origin::Create(GURL("https://idp.com"));
  permission.rp_embedder_origin =
      url::Origin::Create(GURL("https://embedder.com"));
  permission.rp_requester_origin =
      url::Origin::Create(GURL("https://requester.com"));
  permission.chosen_account_id = "account123";
  permission.chosen_account_email = "user@idp.com";
  return permission;
}
}  // namespace

class ActorLoginPermissionServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    identity_test_environment_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  }

  void IssueAccessToken() {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            "access_token", base::Time::Max());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  ActorLoginPermissionServiceImpl service_{
      identity_test_environment_.identity_manager(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_)};
};

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsSendsCorrectRequest) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());
  IssueAccessToken();

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(kTestListUrl, request.url.spec());
  EXPECT_EQ("POST", request.method);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
  std::optional<std::string> auth_header =
      request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization);
  ASSERT_TRUE(auth_header.has_value());
  EXPECT_EQ("Bearer access_token", *auth_header);
  EXPECT_EQ("{\"filters\":[]}", network::GetUploadData(request));
}

TEST_F(ActorLoginPermissionServiceImplTest, ListAllPermissionsReturnsEmpty) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());
  IssueAccessToken();

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestListUrl,
                                                             "{}");

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListPermissionsSendsCorrectRequest) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  std::vector<FederatedOrigins> origins = {
      {url::Origin::Create(GURL("https://embedder1.com")),
       url::Origin::Create(GURL("https://requester1.com"))},
      {url::Origin::Create(GURL("https://embedder2.com")),
       url::Origin::Create(GURL("https://requester2.com"))}};
  service_.ListPermissions(origins, future.GetCallback());
  IssueAccessToken();

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(kTestListUrl, request.url.spec());
  EXPECT_EQ("POST", request.method);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
  EXPECT_EQ(base::test::ParseJson(R"({
              "filters": [
                {
                  "federatedCredentialPermissionFilter": {
                    "matchAffiliatedRequesterOrigins": true,
                    "rpEmbedderOrigin": "https://embedder1.com",
                    "rpRequesterOrigin": "https://requester1.com"
                  }
                },
                {
                  "federatedCredentialPermissionFilter": {
                    "matchAffiliatedRequesterOrigins": true,
                    "rpEmbedderOrigin": "https://embedder2.com",
                    "rpRequesterOrigin": "https://requester2.com"
                  }
                }
              ]
            })"),
            base::test::ParseJson(network::GetUploadData(request)));
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsReturnsPermissions) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());
  IssueAccessToken();

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
  IssueAccessToken();

  test_url_loader_factory_.AddResponse(kTestListUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       ListAllPermissionsHandlesInvalidJson) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());
  IssueAccessToken();

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
  IssueAccessToken();
  IssueAccessToken();

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
  IssueAccessToken();

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
  IssueAccessToken();

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestDeleteUrl,
                                                             "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       DeletePermissionReturnsFalseOnError) {
  base::test::TestFuture<bool> future;
  service_.DeletePermission(url::Origin::Create(GURL("https://embedder.com")),
                            future.GetCallback());
  IssueAccessToken();

  test_url_loader_factory_.AddResponse(kTestDeleteUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_FALSE(future.Get());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       GrantPermissionSendsCorrectRequest) {
  base::test::TestFuture<bool> future;
  service_.GrantPermission(CreateValidPermission(), future.GetCallback());
  IssueAccessToken();

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(kTestUpdateUrl, request.url.spec());
  EXPECT_EQ("POST", request.method);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);

  EXPECT_EQ(base::test::ParseJson(R"({
              "federatedCredentialPermission": {
                "chosenAccountId": "account123",
                "idpOrigin": "https://idp.com",
                "rpEmbedderOrigin": "https://embedder.com",
                "rpRequesterOrigin": "https://requester.com"
              }
            })"),
            base::test::ParseJson(network::GetUploadData(request)));
}

TEST_F(ActorLoginPermissionServiceImplTest,
       GrantPermissionReturnsTrueOnSuccess) {
  base::test::TestFuture<bool> future;
  service_.GrantPermission(CreateValidPermission(), future.GetCallback());
  IssueAccessToken();

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUpdateUrl,
                                                             "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(ActorLoginPermissionServiceImplTest,
       GrantPermissionReturnsFalseOnError) {
  base::test::TestFuture<bool> future;
  service_.GrantPermission(CreateValidPermission(), future.GetCallback());
  IssueAccessToken();

  test_url_loader_factory_.AddResponse(kTestUpdateUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_FALSE(future.Get());
}

}  // namespace actor_login
