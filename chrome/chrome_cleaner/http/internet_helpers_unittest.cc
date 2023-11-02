// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/internet_helpers.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/http/internet_unittest_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(InternetHelpersTest, ParseContentType) {
  const struct {
    const wchar_t* content_type;
    const wchar_t* expected_mime_type;
    const wchar_t* expected_charset;
    const bool expected_had_charset;
    const wchar_t* expected_boundary;
  } tests[] = {
      {L"text/html; charset=utf-8", L"text/html", L"utf-8", true, L""},
      {L"text/html; charset=", L"text/html", L"", true, L""},
      {L"text/html; charset", L"text/html", L"", false, L""},
      {L"text/html; charset='", L"text/html", L"", true, L""},
      {L"text/html; charset='utf-8'", L"text/html", L"utf-8", true, L""},
      {L"text/html; charset=\"utf-8\"", L"text/html", L"utf-8", true, L""},
      {L"text/html; charset =utf-8", L"text/html", L"utf-8", true, L""},
      {L"text/html; charset= utf-8", L"text/html", L"utf-8", true, L""},
      {L"text/html; charset=utf-8 ", L"text/html", L"utf-8", true, L""},
      {L"text/html; boundary=\"WebKit-ada-df-dsf-adsfadsfs\"", L"text/html",
       L"", false, L"\"WebKit-ada-df-dsf-adsfadsfs\""},
      {L"text/html; boundary =\"WebKit-ada-df-dsf-adsfadsfs\"", L"text/html",
       L"", false, L"\"WebKit-ada-df-dsf-adsfadsfs\""},
      {L"text/html; boundary= \"WebKit-ada-df-dsf-adsfadsfs\"", L"text/html",
       L"", false, L"\"WebKit-ada-df-dsf-adsfadsfs\""},
      {L"text/html; boundary= \"WebKit-ada-df-dsf-adsfadsfs\"   ", L"text/html",
       L"", false, L"\"WebKit-ada-df-dsf-adsfadsfs\""},
      {L"text/html; boundary=\"WebKit-ada-df-dsf-adsfadsfs  \"", L"text/html",
       L"", false, L"\"WebKit-ada-df-dsf-adsfadsfs  \""},
      {L"text/html; boundary=WebKit-ada-df-dsf-adsfadsfs", L"text/html", L"",
       false, L"WebKit-ada-df-dsf-adsfadsfs"},
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    std::wstring mime_type;
    std::wstring charset;
    bool had_charset = false;
    std::wstring boundary;
    ParseContentType(tests[i].content_type, &mime_type, &charset, &had_charset,
                     &boundary);
    EXPECT_EQ(tests[i].expected_mime_type, mime_type) << "i=" << i;
    EXPECT_EQ(tests[i].expected_charset, charset) << "i=" << i;
    EXPECT_EQ(tests[i].expected_had_charset, had_charset) << "i=" << i;
    EXPECT_EQ(tests[i].expected_boundary, boundary) << "i=" << i;
  }
}

TEST(InternetHelpersTest, ComposeAndDecomposeUrl) {
  const struct {
    const wchar_t* url;
    const wchar_t* scheme;
    const wchar_t* host;
    uint16_t port;
    const wchar_t* path;
  } tests[] = {
      {L"http://example.com/", L"http", L"example.com", 80, L"/"},
      {L"https://example.com/", L"https", L"example.com", 443, L"/"},
      {L"https://sub.example.com/", L"https", L"sub.example.com", 443, L"/"},
      {L"https://example.com:9999/", L"https", L"example.com", 9999, L"/"},
      {L"http://example.com/a/b/c", L"http", L"example.com", 80, L"/a/b/c"},
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    std::wstring scheme, host, path;
    uint16_t port = 0;
    EXPECT_TRUE(DecomposeUrl(tests[i].url, &scheme, &host, &port, &path))
        << "i=" << i;
    EXPECT_EQ(tests[i].scheme, scheme) << "i=" << i;
    EXPECT_EQ(tests[i].host, host) << "i=" << i;
    EXPECT_EQ(tests[i].port, port) << "i=" << i;
    EXPECT_EQ(tests[i].path, path) << "i=" << i;
    EXPECT_EQ(tests[i].url,
              ComposeUrl(tests[i].host, tests[i].port, tests[i].path,
                         tests[i].scheme == std::wstring(L"https")));
  }

  const wchar_t* invalid_urls[] = {
      L"",         L"example.com",       L"example.com/foo",
      L"/foo/bar", L"example.com:80",    L"http://",
      L"http:",    L"http:/example.com", L"http:example.com"};

  for (size_t i = 0; i < std::size(invalid_urls); ++i) {
    std::wstring scheme, host, path;
    uint16_t port = 0;
    EXPECT_FALSE(DecomposeUrl(invalid_urls[i], &scheme, &host, &port, &path))
        << "i=" << i;
  }
}

TEST(InternetHelpersTest, GenerateMultipartHttpRequestBoundary) {
  std::wstring boundary1 = GenerateMultipartHttpRequestBoundary();
  std::wstring boundary2 = GenerateMultipartHttpRequestBoundary();
  EXPECT_FALSE(boundary1.empty());
  EXPECT_FALSE(boundary2.empty());
  EXPECT_NE(boundary1, boundary2);
  ASSERT_EQ(std::wstring::npos,
            boundary1.find_first_not_of(L"-0123456789abcdefABCDEF"));
}

TEST(InternetHelpersTest, GenerateMultipartHttpRequestContentTypeHeader) {
  std::wstring boundary = GenerateMultipartHttpRequestBoundary();
  std::wstring content_type_header =
      GenerateMultipartHttpRequestContentTypeHeader(boundary);

  size_t semicolon = content_type_header.find(L':');
  ASSERT_NE(std::wstring::npos, semicolon);

  std::wstring mime_type, charset, parsed_boundary;
  bool had_charset = false;
  ParseContentType(std::wstring(content_type_header.begin() + semicolon + 1,
                                content_type_header.end()),
                   &mime_type, &charset, &had_charset, &parsed_boundary);
  EXPECT_EQ(boundary, parsed_boundary);
  EXPECT_TRUE(charset.empty());
  EXPECT_FALSE(had_charset);
  EXPECT_EQ(L"multipart/form-data", mime_type);
}

TEST(InternetHelpersTest, GenerateMultipartHttpRequestBody) {
  std::map<std::wstring, std::wstring> parameters;
  parameters[L"param"] = L"value";
  std::wstring boundary = GenerateMultipartHttpRequestBoundary();
  std::string file = "file contents";
  std::wstring file_part_name = L"file_name";

  std::string body = GenerateMultipartHttpRequestBody(parameters, file,
                                                      file_part_name, boundary);
  ExpectMultipartMimeMessageIsPlausible(boundary, parameters, file,
                                        base::WideToUTF8(file_part_name), body);
}

}  // namespace chrome_cleaner
