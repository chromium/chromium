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

// TODO(b/294793274): Stop sharing embedded test server with other services to
// obsolete this check.
bool IsKidsManagementApiRequest(base::StringPiece path) {
  static std::vector<FetcherConfig> configs{kClassifyUrlConfig,
                                            kListFamilyMembersConfig,
                                            kCreatePermissionRequestConfig};
  for (const FetcherConfig& config : configs) {
    if (config.service_path == path) {
      return true;
    }
  }
  return false;
}

kids_chrome_management::ClassifyUrlResponse ClassifyUrlResponse(
    kids_chrome_management::ClassifyUrlResponse::DisplayClassification
        classification) {
  kids_chrome_management::ClassifyUrlResponse response;
  response.set_display_classification(classification);
  return response;
}
}  // namespace

void SetHttpEndpointsForKidsManagementApis(
    base::test::ScopedFeatureList& feature_list,
    base::StringPiece endpoint) {
  feature_list.InitAndEnableFeatureWithParameters(
      kSupervisedUserProtoFetcherConfig,
      {{"service_endpoint", base::StrCat({"http://", endpoint})}});
}

KidsManagementApiServerMock::KidsManagementApiServerMock() = default;
KidsManagementApiServerMock::~KidsManagementApiServerMock() {
  // Without this check, some tests could silently pass or fail without
  // interacting this mock. Strict accounting ensures that all expected
  // classifications have happened.
  CHECK(classifications_.empty())
      << "All expected classifications must be exhausted.";
}

void KidsManagementApiServerMock::InstallOn(
    raw_ptr<net::test_server::EmbeddedTestServer> test_server_) {
  CHECK(!test_server_->Started());

  test_server_->RegisterRequestHandler(base::BindRepeating(
      &KidsManagementApiServerMock::ClassifyUrl, base::Unretained(this)));
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &KidsManagementApiServerMock::ListFamilyMembers, base::Unretained(this)));

  test_server_->RegisterRequestMonitor(base::BindRepeating(
      &KidsManagementApiServerMock::RequestMonitorDispatcher,
      base::Unretained(this)));
}

// Return a default Simpson family.
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

// Allow urls according to queue of classifications.
std::unique_ptr<net::test_server::HttpResponse>
KidsManagementApiServerMock::ClassifyUrl(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != kClassifyUrlConfig.service_path) {
    return nullptr;
  }

  CHECK(!classifications_.empty()) << "Expected classification.";

  kids_chrome_management::ClassifyUrlResponse::DisplayClassification
      classification = classifications_.front();
  classifications_.pop_front();

  return FromProtoData(ClassifyUrlResponse(classification).SerializeAsString());
}

void KidsManagementApiServerMock::QueueAllowedUrlClassification() {
  QueueUrlClassification(kids_chrome_management::ClassifyUrlResponse::ALLOWED);
}
void KidsManagementApiServerMock::QueueRestrictedUrlClassification() {
  QueueUrlClassification(
      kids_chrome_management::ClassifyUrlResponse::RESTRICTED);
}

void KidsManagementApiServerMock::QueueUrlClassification(
    kids_chrome_management::ClassifyUrlResponse::DisplayClassification
        display_classification) {
  classifications_.push_back(display_classification);
}

base::CallbackListSubscription KidsManagementApiServerMock::Subscribe(
    base::RepeatingCallback<RequestMonitor> monitor) {
  return request_monitors_.Add(monitor);
}

void KidsManagementApiServerMock::RequestMonitorDispatcher(
    const net::test_server::HttpRequest& request) {
  if (!IsKidsManagementApiRequest(request.GetURL().path())) {
    return;
  }

  request_monitors_.Notify(request.GetURL().path(), request.content);
}

}  // namespace supervised_user
