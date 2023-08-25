// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/printing/uri.h"
#include "chromeos/printing/uri_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using UriComponents = uri_unittest::UriComponents;

// Returns true <=> |c| belongs to STD_CHARS.
bool IsStdChar(char c) {
  if (c >= 'A' && c <= 'Z')
    return true;
  if (c >= 'a' && c <= 'z')
    return true;
  if (c >= '0' && c <= '9')
    return true;
  return (c == '-' || c == '.' || c == '_' || c == '~' || c == '!' ||
          c == '$' || c == '\'' || c == '(' || c == ')' || c == '*' ||
          c == ',' || c == ';');
}

// Returns a copy of |input| where all characters outside the set
// {STD_CHARS + |allowed_schars|} are replaced by %-escaped sequences.
std::string Encode(const std::string& input, const std::string& allowed_chars) {
  std::string out;
  for (char c : input) {
    if (IsStdChar(c) || base::Contains(allowed_chars, c)) {
      out.push_back(c);
    } else {
      out.push_back('%');
      out.append(base::HexEncode(&c, 1));
    }
  }
  return out;
}

// A version of Encode function for a different parameter type.
std::vector<std::string> Encode(const std::vector<std::string>& input,
                                const std::string& allowed_chars) {
  std::vector<std::string> v;
  for (auto& s : input)
    v.push_back(Encode(s, allowed_chars));
  return v;
}

// A version of Encode function for a different parameter type.
std::vector<std::pair<std::string, std::string>> Encode(
    const std::vector<std::pair<std::string, std::string>>& input,
    const std::string& allowed_chars) {
  std::vector<std::pair<std::string, std::string>> v;
  for (auto& p : input)
    v.push_back(std::make_pair(Encode(p.first, allowed_chars),
                               Encode(p.second, allowed_chars)));
  return v;
}

// This test suite consists of tests accepting a single parameter of type
// UriComponents. Each test creates Uri object from the parameter and checks
// its consistency by comparing results returned by different methods.
class UriConsistencyTest : public testing::TestWithParam<UriComponents> {
 public:
  void SetUp() override {
    const UriComponents& components = GetParam();
    uri_.SetFragment(components.fragment);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
    uri_.SetHost(components.host);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
    uri_.SetPath(components.path);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
    uri_.SetPort(components.port);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
    uri_.SetQuery(components.query);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
    uri_.SetScheme(components.scheme);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
    uri_.SetUserinfo(components.userinfo);
    ASSERT_EQ(uri_.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  }

 protected:
  Uri uri_;
};

// Make sure that components returned by Get*Encoded() methods are %-escaped
// versions of components returned by corresponding Get*() methods.
TEST_P(UriConsistencyTest, ComponentsEncoding) {
  EXPECT_EQ(uri_.GetUserinfoEncoded(), Encode(uri_.GetUserinfo(), "+&=:"));
  EXPECT_EQ(uri_.GetHostEncoded(), Encode(uri_.GetHost(), "+&="));
  EXPECT_EQ(uri_.GetPathEncoded(), Encode(uri_.GetPath(), "+&=:@"));
  EXPECT_EQ(uri_.GetQueryEncoded(), Encode(uri_.GetQuery(), ":@/?"));
  EXPECT_EQ(uri_.GetFragmentEncoded(), Encode(uri_.GetFragment(), "+&=:@/?"));
}

// Build Path and verify GetPathEncodedAsString().
TEST_P(UriConsistencyTest, PathBuilding) {
  std::string expected_path;
  for (auto& segment : uri_.GetPathEncoded())
    expected_path += "/" + segment;
  EXPECT_EQ(expected_path, uri_.GetPathEncodedAsString());
}

// Build Query and verify GetQueryEncodedAsString().
TEST_P(UriConsistencyTest, QueryBuilding) {
  std::string expected_query;
  for (auto& param_value : uri_.GetQueryEncoded()) {
    if (!expected_query.empty())
      expected_query += "&";
    expected_query += param_value.first;
    if (!param_value.second.empty())
      expected_query += "=" + param_value.second;
  }
  EXPECT_EQ(expected_query, uri_.GetQueryEncodedAsString());
}

// Build normalized URI from encoded components and make sure that it is
// equal to the value returned by GetNormalized().
TEST_P(UriConsistencyTest, UriBuilding) {
  std::string scheme = uri_.GetScheme();
  if (!scheme.empty())
    scheme += ":";

  // Build a part of URI called Authority (Userinfo@Host:Port).
  std::string authority_encoded;
  if (!uri_.GetUserinfoEncoded().empty())
    authority_encoded = uri_.GetUserinfoEncoded() + "@";
  authority_encoded += uri_.GetHostEncoded();
  std::string authority_with_port_encoded = authority_encoded;
  if (uri_.GetPort() != -1) {
    if (uri_.GetPort() != Uri::GetDefaultPort(uri_.GetScheme()))
      authority_encoded += ":" + base::NumberToString(uri_.GetPort());
    authority_with_port_encoded += ":" + base::NumberToString(uri_.GetPort());
  }
  // If Authority is not empty, prefix it with "//".
  if (!authority_encoded.empty())
    authority_encoded = "//" + authority_encoded;
  if (!authority_with_port_encoded.empty())
    authority_with_port_encoded = "//" + authority_with_port_encoded;

  // Build Path, Query and Fragment.
  std::string path_query_fragment_encoded = uri_.GetPathEncodedAsString();
  if (!uri_.GetQueryEncodedAsString().empty())
    path_query_fragment_encoded += "?" + uri_.GetQueryEncodedAsString();
  if (!uri_.GetFragmentEncoded().empty())
    path_query_fragment_encoded += "#" + uri_.GetFragmentEncoded();

  EXPECT_EQ(uri_.GetNormalized(false),
            scheme + authority_encoded + path_query_fragment_encoded);
  EXPECT_EQ(uri_.GetNormalized(true),
            scheme + authority_with_port_encoded + path_query_fragment_encoded);
}

// Checks if the normalization algorithm is consistent.
TEST_P(UriConsistencyTest, Normalization) {
  // Normalization of normalized uri must not change it.
  Uri uri2(uri_.GetNormalized());
  EXPECT_EQ(uri2.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri_.GetNormalized(true), uri2.GetNormalized(true));
  EXPECT_EQ(uri_.GetNormalized(false), uri2.GetNormalized(false));

  // Normalization of normalized Path must not change it.
  uri2.SetPathEncoded(uri_.GetPathEncodedAsString());
  EXPECT_EQ(uri2.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri_.GetPath(), uri2.GetPath());

  // Normalization of normalized Query must not change it.
  uri2.SetQueryEncoded(uri_.GetQueryEncodedAsString());
  EXPECT_EQ(uri2.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri_.GetQuery(), uri2.GetQuery());
}

INSTANTIATE_TEST_SUITE_P(
    UriConsistencyTestInstantiation,
    UriConsistencyTest,
    testing::Values(
        UriComponents(),  // empty URI
        UriComponents("ExAmplE+SchemA-X",
                      "",
                      "ExAmplE.COM",
                      123,
                      {"D", "E"},
                      {{"F", "G"}, {"H", "I"}},
                      "J"),
        UriComponents("",
                      kPrintableASCII,
                      kPrintableASCII,
                      0,
                      {kPrintableASCII},
                      {{kPrintableASCII, kPrintableASCII}},
                      kPrintableASCII),
        UriComponents("A+1-b.C", "", "", -1, {"//", " "}, {}, "?#@/"),
        UriComponents("http",
                      "",
                      "utf8.test",
                      -1,
                      {},
                      {{"zażółć", "za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87"},
                       {"gęślą", "\x67\xC4\x99\xC5\x9B\x6C\xC4\x85"},
                       {"jaźń", "ja\xc5\xba\xc5\x84"}})));

}  // namespace
}  // namespace chromeos
