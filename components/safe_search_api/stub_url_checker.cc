// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/stub_url_checker.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace safe_search_api {

namespace {

constexpr char kSafeSearchApiUrl[] =
    "https://safesearch.googleapis.com/v1:classify";

std::string BuildResponse(bool is_porn) {
  base::Value::Dict dict;
  base::Value::Dict classification_dict;
  if (is_porn)
    classification_dict.Set("pornography", is_porn);
  base::Value::List classifications_list;
  classifications_list.Append(std::move(classification_dict));
  dict.Set("classifications", std::move(classifications_list));
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

}  // namespace

StubURLChecker::StubURLChecker()
    : test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

StubURLChecker::~StubURLChecker() = default;

std::unique_ptr<URLChecker> StubURLChecker::BuildURLChecker(size_t cache_size) {
  return std::make_unique<URLChecker>(
      std::make_unique<SafeSearchURLCheckerClient>(
          test_shared_loader_factory_, TRAFFIC_ANNOTATION_FOR_TESTS),
      cache_size);
}

void StubURLChecker::SetUpValidResponse(bool is_porn) {
  SetUpResponse(net::OK, BuildResponse(is_porn));
}

void StubURLChecker::SetUpFailedResponse() {
  SetUpResponse(net::ERR_ABORTED, std::string());
}

void StubURLChecker::ClearResponses() {
  test_url_loader_factory_.ClearResponses();
}

void StubURLChecker::SetUpResponse(net::Error error,
                                   const std::string& response) {
  network::URLLoaderCompletionStatus status(error);
  status.decoded_body_length = response.size();
  test_url_loader_factory_.AddResponse(GURL(kSafeSearchApiUrl),
                                       network::mojom::URLResponseHead::New(),
                                       response, status);
}

}  // namespace safe_search_api
