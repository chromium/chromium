// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/fake_annotation_index_server.h"

#include "base/functional/bind.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace multistep_filter {

namespace {

constexpr char kExtractTaskAttributesEndpoint[] = "ExtractTaskAttributes";
constexpr char kGetSupportedTasksEndpoint[] = "GetSupportedTasks";
constexpr char kGetTaskExecutionStrategiesEndpoint[] =
    "GetTaskExecutionStrategies";
constexpr char kApplicationProtobufContentType[] = "application/x-protobuf";

template <typename T>
std::unique_ptr<net::test_server::HttpResponse> CreateProtobufResponse(
    const std::optional<T>& proto_response,
    net::HttpStatusCode status) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(status);
  response->set_content(proto_response ? proto_response->SerializeAsString()
                                       : "");
  response->set_content_type(std::string(kApplicationProtobufContentType));
  return response;
}

}  // namespace

FakeAnnotationIndexServer::FakeAnnotationIndexServer() = default;
FakeAnnotationIndexServer::~FakeAnnotationIndexServer() = default;

void FakeAnnotationIndexServer::Initialize(
    net::test_server::EmbeddedTestServer* test_server) {
  test_server->RegisterRequestHandler(base::BindRepeating(
      &FakeAnnotationIndexServer::HandleRequest, base::Unretained(this)));
}

void FakeAnnotationIndexServer::SetExtractResponse(
    const std::optional<ExtractTaskAttributesResponse>& response,
    net::HttpStatusCode status) {
  extract_response_ = response;
  extract_status_ = status;
}

void FakeAnnotationIndexServer::SetSupportedTasksResponse(
    const std::optional<GetSupportedTasksResponse>& response,
    net::HttpStatusCode status) {
  supported_tasks_response_ = response;
  supported_tasks_status_ = status;
}

void FakeAnnotationIndexServer::SetExecutionStrategiesResponse(
    const std::optional<GetTaskExecutionStrategiesResponse>& response,
    net::HttpStatusCode status) {
  execution_strategies_response_ = response;
  execution_strategies_status_ = status;
  last_strategies_request_.reset();
}

const std::optional<GetTaskExecutionStrategiesRequest>&
FakeAnnotationIndexServer::GetLastStrategiesRequest() const {
  return last_strategies_request_;
}

std::unique_ptr<net::test_server::HttpResponse>
FakeAnnotationIndexServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL() ==
      request.base_url.Resolve(kExtractTaskAttributesEndpoint)) {
    return CreateProtobufResponse(extract_response_, extract_status_);
  } else if (request.GetURL() ==
             request.base_url.Resolve(kGetSupportedTasksEndpoint)) {
    return CreateProtobufResponse(supported_tasks_response_,
                                  supported_tasks_status_);
  } else if (request.GetURL() ==
             request.base_url.Resolve(kGetTaskExecutionStrategiesEndpoint)) {
    std::optional<GetTaskExecutionStrategiesResponse> response =
        execution_strategies_response_;
    GetTaskExecutionStrategiesRequest parsed_request;
    if (parsed_request.ParseFromString(request.content)) {
      last_strategies_request_ = parsed_request;
      if (response) {
        if (parsed_request.candidates_size() > 0 &&
            response->execution_strategies_size() > 0) {
          response->mutable_execution_strategies(0)->set_candidate_id(
              parsed_request.candidates(0).candidate_id());
        }
      }
    }
    return CreateProtobufResponse(response, execution_strategies_status_);
  }
  return nullptr;
}

}  // namespace multistep_filter
