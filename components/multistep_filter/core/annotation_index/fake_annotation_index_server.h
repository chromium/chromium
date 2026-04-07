// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_FAKE_ANNOTATION_INDEX_SERVER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_FAKE_ANNOTATION_INDEX_SERVER_H_

#include <memory>
#include <optional>
#include <vector>

#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "net/http/http_status_code.h"

namespace net::test_server {
class EmbeddedTestServer;
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

namespace multistep_filter {

// Fakes the `SiteAutomationIndexServer` network endpoints, allowing tests to
// configure mocked responses for protobuf API requests.
// TODO(crbug.com/498886966): Currently only supporting testing one
// candidate and one strategy at a time. This will be refactored once
// the server API changes to send instruction sets rather than the
// execution strategies.
class FakeAnnotationIndexServer {
 public:
  FakeAnnotationIndexServer();
  ~FakeAnnotationIndexServer();

  FakeAnnotationIndexServer(const FakeAnnotationIndexServer&) = delete;
  FakeAnnotationIndexServer& operator=(const FakeAnnotationIndexServer&) =
      delete;

  void Initialize(net::test_server::EmbeddedTestServer* test_server);

  void SetExtractResponse(
      const std::optional<ExtractTaskAttributesResponse>& response,
      net::HttpStatusCode status = net::HTTP_OK);

  void SetSupportedTasksResponse(
      const std::optional<GetSupportedTasksResponse>& response,
      net::HttpStatusCode status = net::HTTP_OK);

  void SetExecutionStrategiesResponse(
      const std::optional<GetTaskExecutionStrategiesResponse>& response,
      net::HttpStatusCode status = net::HTTP_OK);

  const std::optional<GetTaskExecutionStrategiesRequest>&
  GetLastStrategiesRequest() const;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::optional<ExtractTaskAttributesResponse> extract_response_;
  net::HttpStatusCode extract_status_ = net::HTTP_OK;
  std::optional<GetSupportedTasksResponse> supported_tasks_response_;
  net::HttpStatusCode supported_tasks_status_ = net::HTTP_OK;
  std::optional<GetTaskExecutionStrategiesResponse>
      execution_strategies_response_;
  net::HttpStatusCode execution_strategies_status_ = net::HTTP_OK;
  std::optional<GetTaskExecutionStrategiesRequest> last_strategies_request_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_FAKE_ANNOTATION_INDEX_SERVER_H_
