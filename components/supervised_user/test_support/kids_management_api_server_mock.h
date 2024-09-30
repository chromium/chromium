// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace supervised_user {

extern const std::multimap<kidsmanagement::FamilyRole, std::string>
    kSimpsonFamily;

// Configures the scoped feature list so that the related feature is initialized
// with right parameters to divert kids management api traffic to an http
// endpoint. See supervised_user::FetcherConfig::service_endpoint for details.
void SetHttpEndpointsForKidsManagementApis(
    base::test::ScopedFeatureList& feature_list,
    std::string_view hostname);

// Component of `KidsManagementApiServerMock`. Implements ClassifyUrl as both
// mock and fake, allowing to account the calls but also providing a default
// response (allow or restrict) for every request. Newly created instance has
// purportedly unconfigured default response and will crash on first use - this
// is to limit unintended / unnoticed uses of the mock during test.
class KidsManagementClassifyUrlMock {
 public:
  KidsManagementClassifyUrlMock();
  ~KidsManagementClassifyUrlMock();

  MOCK_METHOD(kidsmanagement::ClassifyUrlResponse::DisplayClassification,
              ClassifyUrl,
              (const net::test_server::HttpRequest& request));
  void set_display_classification(
      kidsmanagement::ClassifyUrlResponse::DisplayClassification
          classification);

 private:
  // The classification response for every request. Needs to be set (see
  // `set_display_classification`) before first use.
  std::optional<kidsmanagement::ClassifyUrlResponse::DisplayClassification>
      display_classification_;
};

// Simplified implementation of the real Kids Management API server, purposed to
// serve as request handlers for the net::test_server::EmbeddedTestServer.
class KidsManagementApiServerMock {
 public:
  // Introduce a signature that is nicer to use with gtest/gmock expectations.
  using RequestMonitor = void(std::string_view request_path,
                              std::string_view request_content);

  KidsManagementApiServerMock();
  KidsManagementApiServerMock(KidsManagementApiServerMock&& other) = delete;
  KidsManagementApiServerMock& operator=(KidsManagementApiServerMock&& other) =
      delete;
  ~KidsManagementApiServerMock();

  // Installs this mock on a given Embedded Test Server. The server must outlive
  // this instance and must not be started prior to calling this method.
  // Caution: installed handlers are executed until one matches the request.
  void InstallOn(net::test_server::EmbeddedTestServer& test_server_);

  // Subscribes a monitor to this api server. The monitor will be notified about
  // every request, to all of its endpoints.
  base::CallbackListSubscription Subscribe(
      base::RepeatingCallback<RequestMonitor> monitor);

  // Set the mock to respond with allow or restrict url for all subsequent
  // requests to ClassifyUrl.
  void AllowSubsequentClassifyUrl();
  void RestrictSubsequentClassifyUrl();

  KidsManagementClassifyUrlMock& classify_url_mock() {
    return classify_url_mock_;
  }

 private:
  // Api handler for /kidsmanagement/v1/people/me:classifyUrl
  std::unique_ptr<net::test_server::HttpResponse> ClassifyUrl(
      const net::test_server::HttpRequest& request);
  // Api handler for /kidsmanagement/v1/families/mine/members
  std::unique_ptr<net::test_server::HttpResponse> ListFamilyMembers(
      const net::test_server::HttpRequest& request);

  void RequestMonitorDispatcher(const net::test_server::HttpRequest& request);

  base::RepeatingCallbackList<RequestMonitor> request_monitors_;

  KidsManagementClassifyUrlMock classify_url_mock_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_
