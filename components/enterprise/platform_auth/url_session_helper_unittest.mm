// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/platform_auth/url_session_helper.h"

#import <Foundation/Foundation.h>

#include <cstring>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace url_session_helper {

namespace {

NSData* CreateNSDataFromString(const std::string& s) {
  return [NSData dataWithBytes:s.data() length:s.size()];
}

scoped_refptr<network::ResourceRequestBody> CreateBodyFromString(
    absl::string_view content) {
  auto span = base::as_byte_span(content);
  return network::ResourceRequestBody::CreateFromBytes(
      std::vector<uint8_t>(span.begin(), span.end()));
}

constexpr char kUrl[] = "https://example.com/";
constexpr char kMethod[] = "POST";
constexpr char kInitiator[] = "https://initiator.com";
const NSString* kNsInitiator = base::SysUTF8ToNSString(kInitiator);
constexpr char kContent[] = "payload";
constexpr char kOrigin[] = "Origin";
constexpr char kAllowHeaders[] = "Access-Control-Allow-Headers";
constexpr char kAllowOrigin[] = "Access-Control-Allow-Origin";
constexpr base::TimeDelta kTimeout = base::Seconds(2);
constexpr char kInvalidString[] = "\x80";

constexpr char kAllowedRequestHeader1[] = "Accept";
constexpr char kAllowedRequestHeader2[] = "User-Agent";
constexpr char kBannedRequestHeader1[] = "access-control-allow-credentials";
constexpr char kBannedRequestHeader2[] = "x-content-type-options";

constexpr char kAllowedResponseHeader1[] = "access-control-allow-credentials";
constexpr char kAllowedResponseHeader2[] = "x-content-type-options";
constexpr char kBannedResponseHeader1[] = "User-Agent";

constexpr char kContentType[] = "Content-Type";
constexpr char kContentLength[] = "Content-Length";

constexpr char kValidHeaderValue1[] = "value";
constexpr char kValidHeaderValue2[] = "another_value";
constexpr char kInvalidHeaderName[] = "Invalid@Name";

// This accounts for the size of kOktaSsoFixedRequestHeaders + 1 for the Origin.
constexpr size_t kFixedHeadersCount = 7;

url::Origin GetInitiator() {
  return url::Origin::Create(GURL(kInitiator));
}

}  // namespace

TEST(UrlSessionHelperTest, ConvertResourceRequest_NullRequestBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(nil, result.HTTPBody);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_SimpleBytesBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_body = CreateBodyFromString(kContent);
  request.request_initiator = GetInitiator();
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  ASSERT_NE(nil, result.HTTPBody);
  EXPECT_TRUE([result.HTTPBody
      isEqualToData:CreateNSDataFromString(std::string(kContent))]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_EmptyBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request.request_initiator = GetInitiator();
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(0u, result.HTTPBody.length);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_UnsupportedBodyType) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("filename.txt")), 0,
                        10, base::Time());
  request.request_body = std::move(body);
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(nil, result.HTTPBody);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_BasicHeaders) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  net::HttpRequestHeaders headers;
  headers.SetHeader(kAllowedRequestHeader1, kValidHeaderValue1);
  headers.SetHeader(kAllowedRequestHeader2, kValidHeaderValue2);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(2 + kFixedHeadersCount, result.allHTTPHeaderFields.count);
  EXPECT_NSEQ(@(kValidHeaderValue1),
              result.allHTTPHeaderFields[@(kAllowedRequestHeader1)]);
  EXPECT_NSEQ(@(kValidHeaderValue2),
              result.allHTTPHeaderFields[@(kAllowedRequestHeader2)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_SkipsInvalidHeaders) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  net::HttpRequestHeaders headers;
  headers.SetHeader(kAllowedRequestHeader1, kValidHeaderValue1);
  headers.SetHeader(kAllowedRequestHeader2, kInvalidString);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(1u + kFixedHeadersCount, result.allHTTPHeaderFields.count);
  EXPECT_NSEQ(@(kValidHeaderValue1),
              result.allHTTPHeaderFields[@(kAllowedRequestHeader1)]);
}

TEST(UrlSessionHelperTest,
     ConvertResourceRequest_SkipsNotAllowlistedRequestHeaders) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  net::HttpRequestHeaders headers;
  headers.SetHeader(kAllowedRequestHeader1, kValidHeaderValue1);
  headers.SetHeader(kAllowedRequestHeader2, kValidHeaderValue2);
  headers.SetHeader(kBannedRequestHeader1, kValidHeaderValue1);
  headers.SetHeader(kBannedRequestHeader2, kValidHeaderValue2);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(2u + kFixedHeadersCount, result.allHTTPHeaderFields.count);
  EXPECT_NSEQ(@(kValidHeaderValue1),
              result.allHTTPHeaderFields[@(kAllowedRequestHeader1)]);
  EXPECT_NSEQ(@(kValidHeaderValue2),
              result.allHTTPHeaderFields[@(kAllowedRequestHeader2)]);
  EXPECT_EQ(nil, result.allHTTPHeaderFields[@(kBannedRequestHeader1)]);
  EXPECT_EQ(nil, result.allHTTPHeaderFields[@(kBannedRequestHeader2)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_EmptyResult) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  net::HttpRequestHeaders headers;
  headers.SetHeader(kAllowedRequestHeader1, kInvalidString);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(kFixedHeadersCount, result.allHTTPHeaderFields.count);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_BasicFields) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.headers.SetHeader(kAllowedRequestHeader1, kValidHeaderValue1);
  request.headers.SetHeader(kAllowedRequestHeader2, kValidHeaderValue2);
  request.request_initiator = GetInitiator();

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_NSEQ([NSURL URLWithString:@(kUrl)], result.URL);
  EXPECT_NSEQ(@(kMethod), result.HTTPMethod);
  EXPECT_EQ(kTimeout.InSeconds(), result.timeoutInterval);

  NSDictionary* headers = result.allHTTPHeaderFields;
  EXPECT_NSEQ(@(kValidHeaderValue1), headers[@(kAllowedRequestHeader1)]);
  EXPECT_NSEQ(@(kValidHeaderValue2), headers[@(kAllowedRequestHeader2)]);
  EXPECT_NSEQ(@(kInitiator), headers[@(kOrigin)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithFieldsHeadersAndBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.headers.SetHeader(kAllowedRequestHeader1, kValidHeaderValue1);
  request.request_initiator = GetInitiator();
  request.request_body = CreateBodyFromString(kContent);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);

  EXPECT_NSEQ([NSURL URLWithString:@(kUrl)], result.URL);
  EXPECT_NSEQ(@(kMethod), result.HTTPMethod);
  EXPECT_EQ(kTimeout.InSeconds(), result.timeoutInterval);

  NSDictionary* headers = result.allHTTPHeaderFields;
  EXPECT_NSEQ(@(kValidHeaderValue1), headers[@(kAllowedRequestHeader1)]);
  EXPECT_NSEQ(@(kInitiator), headers[@(kOrigin)]);

  ASSERT_NE(nil, result.HTTPBody);
  EXPECT_TRUE([result.HTTPBody isEqualToData:CreateNSDataFromString(kContent)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithInvalidURL) {
  network::ResourceRequest request;
  request.url = GURL();
  request.request_initiator = GetInitiator();
  request.method = kMethod;
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  EXPECT_EQ(nil, result);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithInvalidMethod) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kInvalidString;
  request.request_initiator = GetInitiator();
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  EXPECT_EQ(nil, result);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_AllowedHeadersAreNormalised) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_initiator = GetInitiator();
  request.headers.SetHeader("Content-Type", "text");
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  EXPECT_NSEQ(@("text"), result.allHTTPHeaderFields[@("Content-Type")]);

  network::ResourceRequest request_lower_case;
  request_lower_case.url = GURL(kUrl);
  request_lower_case.method = kMethod;
  request_lower_case.request_initiator = GetInitiator();
  request_lower_case.headers.SetHeader("content-type", "text");
  NSURLRequest* result_lower_case =
      ConvertResourceRequest(request_lower_case, kTimeout);
  EXPECT_NSEQ(@("text"),
              result_lower_case.allHTTPHeaderFields[@("content-type")]);
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_BasicHttp200) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  const std::string allowed_headers =
      base::JoinString({kAllowedResponseHeader1, kAllowedResponseHeader2,
                        kContentLength, kContentType},
                       ",");
  NSDictionary* headers = @{
    @(kAllowOrigin) : @(kInitiator),
    @(kAllowHeaders) : base::SysUTF8ToNSString(allowed_headers),
    @(kAllowedResponseHeader1) : @(kValidHeaderValue1),
    @(kAllowedResponseHeader2) : @(kValidHeaderValue2),
    @(kContentLength) : @"42",
    @(kContentType) : @("text/json"),
    @(kBannedResponseHeader1) : @(kValidHeaderValue1),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];
  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());

  ASSERT_TRUE(head);
  EXPECT_EQ(head->mime_type, "text/json");
  EXPECT_EQ(head->content_length, 42);
  EXPECT_TRUE(head->network_accessed);

  ASSERT_TRUE(head->headers);
  EXPECT_EQ(head->headers->response_code(), 200);
  EXPECT_EQ(head->headers->GetStatusLine(), "HTTP/1.1 200 OK");
  ASSERT_TRUE(head->headers->HasHeader(kAllowedResponseHeader1));
  EXPECT_EQ(head->headers->GetNormalizedHeader(kAllowedResponseHeader1).value(),
            kValidHeaderValue1);
  EXPECT_EQ(head->headers->GetNormalizedHeader(kAllowedResponseHeader2).value(),
            kValidHeaderValue2);
  EXPECT_FALSE(head->headers->HasHeader(kBannedResponseHeader1));
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_Http404) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kAllowOrigin) : @(kInitiator),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:404
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];

  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());

  ASSERT_TRUE(head);
  ASSERT_TRUE(head->headers);
  EXPECT_EQ(head->headers->response_code(), 404);
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_FiltersInvalidHeaders) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  const std::string allowed_headers =
      std::string(kAllowedResponseHeader1) + "," + kInvalidHeaderName;
  NSDictionary* headers = @{
    @(kAllowOrigin) : @(kInitiator),
    @(kAllowHeaders) : base::SysUTF8ToNSString(allowed_headers),
    @(kAllowedResponseHeader1) : @(kValidHeaderValue1),
    @(kInvalidHeaderName) : @(kValidHeaderValue2),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];

  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());
  ASSERT_TRUE(head->headers);
  EXPECT_TRUE(head->headers->HasHeader(kAllowedResponseHeader1));
  EXPECT_FALSE(head->headers->HasHeader(kInvalidHeaderName));
}

TEST(UrlSessionHelperTest,
     ConvertNSURLResponse_AccessControlAllowHeadersMissing) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kAllowOrigin) : @(kInitiator),
    @(kAllowedResponseHeader1) : @(kValidHeaderValue1),
    @(kAllowedResponseHeader2) : @(kValidHeaderValue2),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];

  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());

  ASSERT_TRUE(head->headers);
  EXPECT_FALSE(head->headers->HasHeader(kAllowedResponseHeader1));
  EXPECT_FALSE(head->headers->HasHeader(kAllowedResponseHeader2));
}

TEST(UrlSessionHelperTest,
     ConvertNSURLResponse_FiltersHeadersByAccessControlAllowHeaders) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kAllowOrigin) : @(kInitiator),
    @(kAllowHeaders) : @(kAllowedResponseHeader1),
    @(kAllowedResponseHeader1) : @(kValidHeaderValue1),
    @(kAllowedResponseHeader2) : @(kValidHeaderValue2),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];

  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());

  ASSERT_TRUE(head->headers);
  EXPECT_TRUE(head->headers->HasHeader(kAllowedResponseHeader1));
  EXPECT_FALSE(head->headers->HasHeader(kAllowedResponseHeader2));
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_BlocksCrossOrigin) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kAllowOrigin) : @("https://not.initiator.com"),
    @(kAllowHeaders) : @(kAllowedResponseHeader1),
    @(kAllowedResponseHeader1) : @(kValidHeaderValue1),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];
  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());
  EXPECT_FALSE(head);
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_RequiresAllowOriginHeader) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kAllowHeaders) : @(kAllowedResponseHeader1),
    @(kAllowedResponseHeader1) : @(kValidHeaderValue1),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];
  network::mojom::URLResponseHeadPtr head =
      ConvertNSURLResponse(response, GetInitiator());
  EXPECT_FALSE(head);
}

}  // namespace url_session_helper
