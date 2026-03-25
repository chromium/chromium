// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/content_analysis_browser_test_base.h"

#include <algorithm>

#include "base/base64.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/connectors.equal.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/connectors.to_value.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors::test {

namespace {

std::string GetHeader(const net::test_server::HttpRequest& request,
                      const std::string& header) {
  size_t content_type_header = request.all_headers.find(header);
  if (content_type_header == std::string::npos) {
    return std::string();
  }

  size_t end = request.all_headers.find("\r\n", content_type_header);
  if (end == std::string::npos) {
    return std::string();
  }

  return request.all_headers.substr(content_type_header,
                                    end - content_type_header);
}

// Extract the boundary value used in the request body from the Content-type
// header.
std::string GetMultipartBoundary(const net::test_server::HttpRequest& request) {
  constexpr char kContentTypeMultipart[] =
      "Content-Type: multipart/related; boundary=";
  std::string boundary = GetHeader(request, kContentTypeMultipart);
  if (boundary.empty()) {
    return std::string();
  }

  return boundary.substr(std::size(kContentTypeMultipart) - 1);
}

// Multipart request bodies have the following format:
// return base::StrCat({"--", boundary_, "\r\n", kDataContentType, "\r\n\r\n",
//                      metadata, "\r\n--", boundary_, "\r\n", kDataContentType,
//                      "\r\n\r\n", data, "\r\n--", boundary_, "--\r\n"});
//
// See multipart_uploader_base.cc for the actual code. The following utils
// extract corresponding parts of the body based on the position of the boundary
// indicators and newlines.
ContentAnalysisRequest GetMultipartMetadata(
    const net::test_server::HttpRequest& request) {
  std::string boundary = GetMultipartBoundary(request);

  // The data is between the double newline after the metadata headers and the
  // second boundary.
  size_t metadata_end = request.content.find("\r\n--" + boundary);
  size_t metadata_start =
      request.content.rfind("\r\n\r\n", metadata_end - 4) + 4;

  std::string base64_metadata =
      request.content.substr(metadata_start, metadata_end - metadata_start);

  std::string metadata;
  base::Base64Decode(base64_metadata, &metadata);

  ContentAnalysisRequest content_analysis_request;
  content_analysis_request.ParseFromString(metadata);

  return content_analysis_request;
}

std::string GetMultipartBody(const net::test_server::HttpRequest& request) {
  std::string boundary = GetMultipartBoundary(request);

  // The data is between the double newline after the data headers and the
  // third boundary.
  size_t data_end = request.content.find("\r\n--" + boundary + "--\r\n");
  size_t data_start = request.content.rfind("\r\n\r\n", data_end - 4) + 4;

  return request.content.substr(data_start, data_end - data_start);
}

AnalysisConnector GetAnalysisConnector(const ContentAnalysisData& data) {
  if (!data.text.empty() || !data.image.empty()) {
    return BULK_DATA_ENTRY;
  } else if (!data.paths.empty()) {
    return FILE_ATTACHED;
  } else if (data.page.IsValid()) {
    return PRINT;
  } else {
    return ANALYSIS_CONNECTOR_UNSPECIFIED;
  }
}

bool IsAuthRequest(const ContentAnalysisRequest& request) {
  return !request.has_request_data();
}

std::unique_ptr<net::test_server::HttpResponse> AllowResponse(
    const ContentAnalysisRequest& request) {
  ContentAnalysisResponse response;
  response.set_request_token(request.request_token());

  if (!IsAuthRequest(request)) {
    for (const auto& tag : request.tags()) {
      auto* result = response.add_results();
      result->set_status(ContentAnalysisResponse::Result::SUCCESS);
      result->set_tag(tag);
    }
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content(response.SerializeAsString());
  return http_response;
}

}  // namespace

ContentAnalysisBrowserTestBase::ContentAnalysisBrowserTestBase(
    net::EmbeddedTestServer* embedded_test_server)
    : embedded_test_server_(embedded_test_server) {}

ContentAnalysisBrowserTestBase::~ContentAnalysisBrowserTestBase() = default;

std::unique_ptr<net::test_server::HttpResponse>
ContentAnalysisBrowserTestBase::HandleRequest(
    const net::test_server::HttpRequest& request) {
  ContentAnalysisRequest content_analysis_request =
      GetMultipartMetadata(request);
  std::string body = GetMultipartBody(request);

  auto expected = std::find_if(
      expected_requests_.begin(), expected_requests_.end(),
      [&content_analysis_request, &body, this](const auto& entry) {
        return MatchesRequest(content_analysis_request, entry.first) &&
               body == entry.second;
      });
  EXPECT_NE(expected, expected_requests_.end())
      << "Unexpected content analysis request: "
      << ToValue(content_analysis_request);
  expected_requests_.erase(expected);

  return AllowResponse(content_analysis_request);
}

void ContentAnalysisBrowserTestBase::ExpectScanningRequest(
    const ContentAnalysisData& data,
    const std::string& body) {
  ContentAnalysisRequest request;
  request.set_analysis_connector(GetAnalysisConnector(data));
  request.set_device_token(ExpectedDeviceToken());

  // The fields set above are the only ones expected from an authorization
  // request, so we can add it to `expected_requests_` immediately if it's not
  // already been done for a paste request.
  if (!paste_auth_request_added_) {
#if BUILDFLAG(IS_CHROMEOS)
    // This field is set in authorization requests only on CrOS.
    request.mutable_client_metadata()->set_is_chrome_os_managed_guest_session(
        false);
#endif  // BUILDFLAG(IS_CHROMEOS)
    expected_requests_.push_back({request, ""});
    paste_auth_request_added_ = true;
  }

  request.set_blocking(data.settings.block_until_verdict ==
                       BlockUntilVerdict::kBlock);
  for (const auto& tag_and_settings : data.settings.tags) {
    request.add_tags(tag_and_settings.first);
  }
  request.set_user_action_requests_count(1);
  request.set_reason(data.reason);

  auto* client_metadata = request.mutable_client_metadata();
  client_metadata->mutable_device()->set_dm_token(ExpectedDeviceToken());
  client_metadata->set_is_chrome_os_managed_guest_session(false);

  auto* request_data = request.mutable_request_data();
  *request_data->mutable_copied_text_source() = data.clipboard_source;
  request_data->set_destination(data.url.spec());
  // TODO(crbug.com/488379628): Add logic for managed profiles.
  request_data->set_email("");
  request_data->set_source(data.clipboard_source.url());
  request_data->set_tab_url(data.url.spec());
  request_data->set_url(data.url.spec());

  // TODO(crbug.com/488379628): Add logic for comparing arbitrary referrer
  // chains with more fields set and more than one entry.
  auto* referrer_chain = request_data->add_referrer_chain();
  referrer_chain->set_is_retargeting(false);
  referrer_chain->set_is_url_removed_by_policy(false);
  referrer_chain->set_maybe_launched_by_external_application(true);
  referrer_chain->set_navigation_initiation(
      safe_browsing::ReferrerChainEntry::BROWSER_INITIATED);
  referrer_chain->set_referrer_url("");
  referrer_chain->set_type(safe_browsing::ReferrerChainEntry::EVENT_URL);
  referrer_chain->set_url(data.url.spec());

  expected_requests_.push_back({request, body});
}

bool ContentAnalysisBrowserTestBase::MatchesRequest(
    ContentAnalysisRequest received_request,
    ContentAnalysisRequest expected_request) {
  // Fields checked before the final return statement were not included in
  // `expected_request` since they were not known at the time it was created.

  // ContentAnalysisRequest::request_token
  if (received_request.request_token().empty()) {
    return false;
  }
  expected_request.set_request_token(received_request.request_token());

  // The fields in the following conditions are not required for auth requests.
  if (!IsAuthRequest(received_request)) {
    // ContentAnalysisRequest::referrer_chain::navigation_time_msec
    if (received_request.request_data().referrer_chain_size() !=
        expected_request.request_data().referrer_chain_size()) {
      return false;
    }
    for (int i = 0; i < received_request.request_data().referrer_chain_size();
         ++i) {
      double navigation_time_ms = received_request.request_data()
                                      .referrer_chain(i)
                                      .navigation_time_msec();
      if (navigation_time_ms <= 0) {
        return false;
      }
      expected_request.mutable_request_data()
          ->mutable_referrer_chain(i)
          ->set_navigation_time_msec(navigation_time_ms);
    }

    // ContentAnalysisRequest::user_action_id
    if (received_request.user_action_id().empty()) {
      return false;
    }
    expected_request.set_user_action_id(received_request.user_action_id());

    // ContentAnalysisRequest::local_ips
    if (received_request.local_ips().empty()) {
      return false;
    }
    for (const auto& local_ip : received_request.local_ips()) {
      expected_request.add_local_ips(local_ip);
    }
  }

  return received_request == expected_request;
}

std::string ContentAnalysisBrowserTestBase::ExpectedDeviceToken() {
  // TODO(crbug.com/488379628): Handle cases where device_token is set with a
  // profile DM token.
#if BUILDFLAG(IS_CHROMEOS)
  return "device_dm_token";
#else
  return "browser_dm_token";
#endif
}

}  // namespace enterprise_connectors::test
