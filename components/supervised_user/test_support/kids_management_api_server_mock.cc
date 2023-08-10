// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/kids_management_api_server_mock.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace supervised_user {

namespace {

std::unique_ptr<net::test_server::HttpResponse> FromProtoData(
    base::StringPiece data) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("application/x-protobuf");
  http_response->set_content(data);
  return http_response;
}

}  // namespace

void SetHttpEndpointsForKidsManagementApis(
    base::test::ScopedFeatureList& feature_list,
    base::StringPiece endpoint) {
  feature_list.InitAndEnableFeatureWithParameters(
      kSupervisedUserProtoFetcherConfig,
      {{"service_endpoint", base::StrCat({"http://", endpoint})}});
}

void KidsManagementApiServerMock::InstallOn(
    base::raw_ptr<net::test_server::EmbeddedTestServer> test_server_) {
  CHECK(!test_server_->Started());
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &KidsManagementApiServerMock::ListFamilyMembers, base::Unretained(this)));
}

std::unique_ptr<net::test_server::HttpResponse>
KidsManagementApiServerMock::ListFamilyMembers(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != kListFamilyMembersConfig.service_path) {
    return nullptr;
  }

  kids_chrome_management::ListFamilyMembersResponse response;
  supervised_user::SetFamilyMemberAttributesForTesting(
      response.add_members(), kids_chrome_management::HEAD_OF_HOUSEHOLD,
      "marge@gmail.com");
  supervised_user::SetFamilyMemberAttributesForTesting(
      response.add_members(), kids_chrome_management::PARENT,
      "homer@gmail.com");
  supervised_user::SetFamilyMemberAttributesForTesting(
      response.add_members(), kids_chrome_management::MEMBER,
      "abraham@gmail.com");
  supervised_user::SetFamilyMemberAttributesForTesting(
      response.add_members(), kids_chrome_management::CHILD, "lisa@gmail.com");
  supervised_user::SetFamilyMemberAttributesForTesting(
      response.add_members(), kids_chrome_management::CHILD, "bart@gmail.com");
  return FromProtoData(response.SerializeAsString());
}

}  // namespace supervised_user
