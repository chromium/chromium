// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/scoped_feature_list.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace supervised_user {

// Configures the scoped feature list so that the related feature is initialized
// with right parameters to divert kids management api traffic to an http
// endpoint.
void SetHttpEndpointsForKidsManagementApis(
    base::test::ScopedFeatureList& feature_list,
    base::StringPiece endpoint);

// Simplified implementation of the real Kids Management API server, purposed to
// serve as request handlers for the net::test_server::EmbeddedTestServer.
class KidsManagementApiServerMock {
 public:
  // Installs this mock on a given Embedded Test Server. The server must outlive
  // this instance and must not be started prior to calling this method.
  void InstallOn(
      base::raw_ptr<net::test_server::EmbeddedTestServer> test_server_);

  // Api handler for /kidsmanagement/v1/families/mine/members
  std::unique_ptr<net::test_server::HttpResponse> ListFamilyMembers(
      const net::test_server::HttpRequest& request);
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_MANAGEMENT_API_SERVER_MOCK_H_
