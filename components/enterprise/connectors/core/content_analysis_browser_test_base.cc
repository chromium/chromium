// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/content_analysis_browser_test_base.h"

#include <algorithm>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/connectors.equal.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/connectors.to_value.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "crypto/secure_hash.h"
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

std::string GetHeaderValue(const net::test_server::HttpRequest& request,
                           const std::string& header_name) {
  std::string header = GetHeader(request, header_name);
  if (header.empty()) {
    return std::string();
  }

  // Remove the header name plus the ':' after it.
  std::string value = header.substr(header_name.size() + 1);
  base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
  return value;
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

ContentAnalysisBrowserTestBase::ExpectedRequest::ExpectedRequest(
    ContentAnalysisRequest request,
    std::string body,
    std::vector<std::string> headers)
    : request(request), body(body), headers(headers) {}
ContentAnalysisBrowserTestBase::ExpectedRequest::ExpectedRequest(
    const ExpectedRequest&) = default;
ContentAnalysisBrowserTestBase::ExpectedRequest::ExpectedRequest(
    ExpectedRequest&&) = default;
ContentAnalysisBrowserTestBase::ExpectedRequest&
ContentAnalysisBrowserTestBase::ExpectedRequest::operator=(
    const ExpectedRequest&) = default;
ContentAnalysisBrowserTestBase::ExpectedRequest&
ContentAnalysisBrowserTestBase::ExpectedRequest::operator=(ExpectedRequest&&) =
    default;
ContentAnalysisBrowserTestBase::ExpectedRequest::~ExpectedRequest() = default;

ContentAnalysisBrowserTestBase::ContentAnalysisBrowserTestBase(
    net::EmbeddedTestServer* embedded_test_server)
    : embedded_test_server_(embedded_test_server) {}

ContentAnalysisBrowserTestBase::~ContentAnalysisBrowserTestBase() {
  EXPECT_TRUE(expected_requests_.empty())
      << "Expected request not received during the test: "
      << ToValue(expected_requests_[0].request) << "\n"
      << expected_requests_[0].body;
}

std::unique_ptr<net::test_server::HttpResponse>
ContentAnalysisBrowserTestBase::HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::string protocol = GetHeaderValue(request, "X-Goog-Upload-Protocol");
  std::string command = GetHeaderValue(request, "X-Goog-Upload-Command");
  if (protocol == "resumable" || !command.empty()) {
    // The command header can include multiple comma-separated values, so
    // `find()` is used to parse it.
    if (command.find("start") != std::string::npos) {
      return HandleResumableMetadataRequest(request);
    } else if (command.find("upload, finalize") != std::string::npos) {
      return HandleResumableContentRequest(request);
    } else {
      EXPECT_TRUE(false) << "Unknown Resumable command: " << command;
      return nullptr;
    }
  } else if (protocol == "multipart") {
    return HandleMultipartRequest(request);
  } else {
    EXPECT_TRUE(false) << "Unknown protocol: " << protocol;
    return nullptr;
  }
}

std::unique_ptr<net::test_server::HttpResponse>
ContentAnalysisBrowserTestBase::HandleResumableMetadataRequest(
    const net::test_server::HttpRequest& request) {
  // TODO(crbug.com/498915066): Add logic to validate headers.

  // The ContentAnalysisRequest is expected to be encoded in base64 in the
  // initial resumable request body.
  std::string base64_metadata = request.content;
  std::string metadata;
  base::TrimWhitespaceASCII(base64_metadata, base::TRIM_ALL, &base64_metadata);
  EXPECT_TRUE(base::Base64Decode(base64_metadata, &metadata));
  ContentAnalysisRequest content_analysis_request;
  EXPECT_TRUE(content_analysis_request.ParseFromString(metadata));

  auto expected = std::find_if(
      expected_requests_.begin(), expected_requests_.end(),
      [&content_analysis_request, &request, this](const auto& entry) {
        for (const std::string& header : entry.headers) {
          if (request.all_headers.find(header) == std::string::npos) {
            return false;
          }
        }
        return MatchesRequest(content_analysis_request, entry.request);
      });

  EXPECT_NE(expected, expected_requests_.end())
      << "Unexpected Resumable Metadata request: " << request.all_headers
      << "\n"
      << ToValue(content_analysis_request);

  // Don't remove `expected` from `expected_requests_` here since we're still
  // expecting the second request with the data to come before the end of the
  // test.

  return SendContentMetadataResponse();
}

std::unique_ptr<net::test_server::HttpResponse>
ContentAnalysisBrowserTestBase::HandleResumableContentRequest(
    const net::test_server::HttpRequest& request) {
  // TODO(crbug.com/498915066): Add logic to validate headers.

  auto expected = std::find_if(
      expected_requests_.begin(), expected_requests_.end(),
      [&request](const auto& entry) { return request.content == entry.body; });

  EXPECT_NE(expected, expected_requests_.end())
      << "Unexpected Resumable Content request: " << request.content;

  ContentAnalysisRequest content_analysis_request = expected->request;
  expected_requests_.erase(expected);
  return AllowResponse(content_analysis_request);
}

std::unique_ptr<net::test_server::HttpResponse>
ContentAnalysisBrowserTestBase::HandleMultipartRequest(
    const net::test_server::HttpRequest& request) {
  // TODO(crbug.com/498915066): Add logic to validate headers.

  ContentAnalysisRequest content_analysis_request =
      GetMultipartMetadata(request);
  std::string body = GetMultipartBody(request);

  auto expected = std::find_if(
      expected_requests_.begin(), expected_requests_.end(),
      [&content_analysis_request, &body, &request, this](const auto& entry) {
        for (const std::string& header : entry.headers) {
          if (request.all_headers.find(header) == std::string::npos) {
            return false;
          }
        }
        return MatchesRequest(content_analysis_request, entry.request) &&
               body == entry.body;
      });

  EXPECT_NE(expected, expected_requests_.end())
      << "Unexpected Multipart request: " << ToValue(content_analysis_request);
  expected_requests_.erase(expected);

  return AllowResponse(content_analysis_request);
}

void ContentAnalysisBrowserTestBase::AddExpectedScanningRequest(
    const ContentAnalysisData& data,
    const std::string& body,
    const std::vector<std::string>& headers) {
  ContentAnalysisRequest request;
  request.set_analysis_connector(GetAnalysisConnector(data));
  // TODO(crbug.com/498915066): Add logic to evaluate more profile management
  // cases instead of just relying on the headers not being empty.
  request.set_device_token(headers.empty() ? ExpectedDeviceToken()
                                           : ExpectedProfileToken());

  // The fields set above are the only ones expected from an authorization
  // request, so we can add it to `expected_requests_` immediately if it's not
  // already been done for a paste request.
  AddAuthRequestIfNeeded(data, request);

  request.set_blocking(data.settings.block_until_verdict ==
                       BlockUntilVerdict::kBlock);
  for (const auto& tag_and_settings : data.settings.tags) {
    request.add_tags(tag_and_settings.first);
  }
  request.set_user_action_requests_count(1);
  if (data.reason != ContentAnalysisRequest::UNKNOWN) {
    request.set_reason(data.reason);
  }

  auto* client_metadata = request.mutable_client_metadata();
  client_metadata->mutable_device()->set_dm_token(ExpectedDeviceToken());
  client_metadata->mutable_profile()->set_dm_token("profile_dm_token");
  client_metadata->mutable_device()->set_dm_token(ExpectedDeviceToken());
  client_metadata->set_is_chrome_os_managed_guest_session(false);

  auto* request_data = request.mutable_request_data();
  if (data.reason == ContentAnalysisRequest::CLIPBOARD_PASTE) {
    *request_data->mutable_copied_text_source() = data.clipboard_source;
  }
  request_data->set_destination(data.url.spec());
  // TODO(crbug.com/498915066): Add better logic for managed profiles.
  bool has_auth_header = false;
  for (const std::string& header : headers) {
    if (header.starts_with("Authorization:")) {
      has_auth_header = true;
    }
  }
  request_data->set_email(
      (has_auth_header || data.settings.per_profile) ? "test@example.com" : "");
  request_data->set_source(data.clipboard_source.url());
  request_data->set_tab_url(data.url.spec());
  request_data->set_url(data.url.spec());
  // TODO(crbug.com/498915066): Add logic for multiple files.
  if (!data.paths.empty()) {
    request_data->set_filename(data.paths[0].AsUTF8Unsafe());
    request_data->set_file_size(body.size());
    request_data->set_destination("");

    auto hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    hash->Update(base::as_byte_span(body));
    std::vector<uint8_t> hash_value(hash->GetHashLength());
    hash->Finish(hash_value);
    request_data->set_digest(base::HexEncode(hash_value));
  }

  // TODO(crbug.com/498915066): Add logic for comparing arbitrary referrer
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

  expected_requests_.emplace_back(request, body, headers);
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

    // ContentAnalysisRequest::request_data::content_type
    // TODO(crbug.com/498915066): Add an enterprise mime type function to call
    // to set this earlier instead of just copying `received_request`'s mime
    // type.
    if (received_request.request_data().has_content_type()) {
      expected_request.mutable_request_data()->set_content_type(
          received_request.request_data().content_type());
    }
  }

  return received_request == expected_request;
}

std::string ContentAnalysisBrowserTestBase::ExpectedDeviceToken() {
  // TODO(crbug.com/498915066): Handle cases where device_token is set with a
  // profile DM token.
#if BUILDFLAG(IS_CHROMEOS)
  return "device_dm_token";
#else
  return "browser_dm_token";
#endif
}

std::string ContentAnalysisBrowserTestBase::ExpectedProfileToken() {
  // TODO(crbug.com/498915066): Handle more complex profile/device management
  // cases.
#if BUILDFLAG(IS_CHROMEOS)
  return "device_dm_token";
#else
  return "profile_dm_token";
#endif
}

void ContentAnalysisBrowserTestBase::AddAuthRequestIfNeeded(
    const ContentAnalysisData& data,
    ContentAnalysisRequest request) {
#if BUILDFLAG(IS_CHROMEOS)
  // This field is set in authorization requests only on CrOS.
  request.mutable_client_metadata()->set_is_chrome_os_managed_guest_session(
      false);
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (!paste_auth_request_added_ &&
      request.analysis_connector() == BULK_DATA_ENTRY) {
    expected_requests_.emplace_back(request, "", std::vector<std::string>());
    paste_auth_request_added_ = true;
  }
  if (!file_attach_auth_request_added_ &&
      request.analysis_connector() == FILE_ATTACHED) {
    expected_requests_.emplace_back(request, "", std::vector<std::string>());
    file_attach_auth_request_added_ = true;
  }
}

std::unique_ptr<net::test_server::HttpResponse>
ContentAnalysisBrowserTestBase::SendContentMetadataResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->AddCustomHeader("X-Goog-Upload-Status", "active");
  response->AddCustomHeader(
      "X-Goog-Upload-Url",
      embedded_test_server_->GetURL("/resumable_upload").spec());
  return response;
}

}  // namespace enterprise_connectors::test
