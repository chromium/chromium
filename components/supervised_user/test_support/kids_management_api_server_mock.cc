// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/kids_management_api_server_mock.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace supervised_user {

const std::multimap<kidsmanagement::FamilyRole, std::string> kSimpsonFamily = {
    {kidsmanagement::HEAD_OF_HOUSEHOLD, "marge@gmail.com"},
    {kidsmanagement::PARENT, "homer@gmail.com"},
    {kidsmanagement::MEMBER, "abraham@gmail.com"},
    {kidsmanagement::CHILD, "lisa@gmail.com"},
    {kidsmanagement::CHILD, "bart@gmail.com"},
};

namespace {

std::unique_ptr<net::test_server::HttpResponse> FromProtoData(
    std::string_view data) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("application/x-protobuf");
  http_response->set_content(data);
  return http_response;
}

kidsmanagement::ClassifyUrlResponse ClassifyUrlResponse(
    kidsmanagement::ClassifyUrlResponse::DisplayClassification classification) {
  kidsmanagement::ClassifyUrlResponse response;
  response.set_display_classification(classification);
  return response;
}
}  // namespace

void SetHttpEndpointsForKidsManagementApis(
    base::test::ScopedFeatureList& feature_list,
    std::string_view hostname) {
  feature_list.InitAndEnableFeatureWithParameters(
      kSupervisedUserProtoFetcherConfig,
      {{"service_endpoint", base::StrCat({"http://", hostname})}});
}

KidsManagementClassifyUrlMock::KidsManagementClassifyUrlMock() {
  ON_CALL(*this, ClassifyUrl)
      .WillByDefault([this](const net::test_server::HttpRequest& request) {
        CHECK(display_classification_.has_value())
            << "Set response value (see `set_display_classification`) before "
               "first use";
        return *display_classification_;
      });
}
KidsManagementClassifyUrlMock::~KidsManagementClassifyUrlMock() = default;

void KidsManagementClassifyUrlMock::set_display_classification(
    kidsmanagement::ClassifyUrlResponse::DisplayClassification classification) {
  display_classification_ = classification;
}

KidsManagementApiServerMock::KidsManagementApiServerMock() = default;
KidsManagementApiServerMock::~KidsManagementApiServerMock() = default;

void KidsManagementApiServerMock::InstallOn(
    net::test_server::EmbeddedTestServer& test_server_) {
  CHECK(!test_server_.Started())
      << "Cannot install handlers onto running server.";

  test_server_.RegisterRequestHandler(base::BindRepeating(
      &KidsManagementApiServerMock::ClassifyUrl, base::Unretained(this)));
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &KidsManagementApiServerMock::ListFamilyMembers, base::Unretained(this)));

  test_server_.RegisterRequestMonitor(base::BindRepeating(
      &KidsManagementApiServerMock::RequestMonitorDispatcher,
      base::Unretained(this)));
}

// Return a default Simpson family.
std::unique_ptr<net::test_server::HttpResponse>
KidsManagementApiServerMock::ListFamilyMembers(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != kListFamilyMembersConfig.StaticServicePath()) {
    return nullptr;
  }

  kidsmanagement::ListMembersResponse response;
  for (const auto& [role, email] : kSimpsonFamily) {
    supervised_user::SetFamilyMemberAttributesForTesting(response.add_members(),
                                                         role, email);
  }

  return FromProtoData(response.SerializeAsString());
}

// Allow urls according to queue of classifications.
std::unique_ptr<net::test_server::HttpResponse>
KidsManagementApiServerMock::ClassifyUrl(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != kClassifyUrlConfig.StaticServicePath()) {
    return nullptr;
  }

  return FromProtoData(
      ClassifyUrlResponse(classify_url_mock_.ClassifyUrl(request))
          .SerializeAsString());
}

base::CallbackListSubscription KidsManagementApiServerMock::Subscribe(
    base::RepeatingCallback<RequestMonitor> monitor) {
  return request_monitors_.Add(monitor);
}

void KidsManagementApiServerMock::RequestMonitorDispatcher(
    const net::test_server::HttpRequest& request) {
  request_monitors_.Notify(request.GetURL().path(), request.content);
}

void KidsManagementApiServerMock::AllowSubsequentClassifyUrl() {
  classify_url_mock_.set_display_classification(
      kidsmanagement::ClassifyUrlResponse::DisplayClassification::
          ClassifyUrlResponse_DisplayClassification_ALLOWED);
}

void KidsManagementApiServerMock::RestrictSubsequentClassifyUrl() {
  classify_url_mock_.set_display_classification(
      kidsmanagement::ClassifyUrlResponse::DisplayClassification::
          ClassifyUrlResponse_DisplayClassification_RESTRICTED);
}

}  // namespace supervised_user
