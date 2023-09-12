// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_

#include <deque>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace supervised_user {

// Configures the scoped feature list so that the related feature is initialized
// with right parameters to divert kids management api traffic to an http
// endpoint. See supervised_user::FetcherConfig::service_endpoint for details.
void SetHttpEndpointsForKidsManagementApis(
    base::test::ScopedFeatureList& feature_list,
    base::StringPiece hostname);

// Simplified implementation of the real Kids Management API server, purposed to
// serve as request handlers for the net::test_server::EmbeddedTestServer.
class KidsManagementApiServerMock {
 public:
  // Introduce a signature that is nicer to use with gtest/gmock expectations.
  using RequestMonitor = void(base::StringPiece request_path,
                              base::StringPiece request_content);

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

  // Number of requests to ClassifyUrl endpoint must strictly match the number
  // of queued classifications.
  void QueueAllowedUrlClassification();
  void QueueRestrictedUrlClassification();

 private:
  void QueueUrlClassification(
      kids_chrome_management::ClassifyUrlResponse::DisplayClassification
          display_classification);
  // Api handler for /kidsmanagement/v1/people/me:classifyUrl
  std::unique_ptr<net::test_server::HttpResponse> ClassifyUrl(
      const net::test_server::HttpRequest& request);
  // Api handler for /kidsmanagement/v1/families/mine/members
  std::unique_ptr<net::test_server::HttpResponse> ListFamilyMembers(
      const net::test_server::HttpRequest& request);

  void RequestMonitorDispatcher(const net::test_server::HttpRequest& request);

  base::RepeatingCallbackList<RequestMonitor> request_monitors_;

  // Queue of pending classifications for
  // /kidsmanagement/v1/people/me:classifyUrl
  std::deque<kids_chrome_management::ClassifyUrlResponse::DisplayClassification>
      classifications_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_
