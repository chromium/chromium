// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/uri_unittest.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/printing/uri.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace uri_unittest {
UriComponents::UriComponents() = default;
UriComponents::UriComponents(const UriComponents&) = default;
UriComponents::UriComponents(
    const std::string& scheme,
    const std::string& userinfo,
    const std::string& host,
    int port,
    const std::vector<std::string>& path,
    const std::vector<std::pair<std::string, std::string>>& query,
    const std::string& fragment)
    : scheme(scheme),
      userinfo(userinfo),
      host(host),
      port(port),
      path(path),
      query(query),
      fragment(fragment) {}
UriComponents::~UriComponents() = default;
}  // namespace uri_unittest

namespace {

using UriComponents = uri_unittest::UriComponents;

// Verifies that |components| set by Set*() methods produces given
// |normalized_uri|. Runs also consistency test on the created Uri object.
void TestBuilder(const UriComponents& components,
                 const std::string& normalized_uri) {
  Uri uri;
  uri.SetFragment(components.fragment);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetHost(components.host);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetPath(components.path);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetPort(components.port);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetQuery(components.query);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetScheme(components.scheme);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetUserinfo(components.userinfo);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  // Check URI.
  EXPECT_EQ(uri.GetNormalized(false), normalized_uri);
}

// Verifies that |input_uri| set as parameter in Uri constructor is parsed
// as |components|. Runs also consistency test on the created Uri object.
void TestParser(const std::string& input_uri, const UriComponents& components) {
  Uri uri(input_uri);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  // Check components values.
  EXPECT_EQ(uri.GetScheme(), components.scheme);
  EXPECT_EQ(uri.GetUserinfo(), components.userinfo);
  EXPECT_EQ(uri.GetHost(), components.host);
  EXPECT_EQ(uri.GetPort(), components.port);
  EXPECT_EQ(uri.GetPath(), components.path);
  EXPECT_EQ(uri.GetQuery(), components.query);
  EXPECT_EQ(uri.GetFragment(), components.fragment);
}

// Verifies that |input_uri| set as parameter in Uri constructor is normalized
// to |normalized_uri|. Runs also consistency test on the created Uri object.
void TestNormalization(const std::string& input_uri,
                       const std::string& normalized_uri) {
  Uri uri(input_uri);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetNormalized(false), normalized_uri);
}

TEST(UriTest, DefaultConstructor) {
  Uri uri;
  EXPECT_EQ(uri.GetNormalized(), "");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetScheme(), "");
  EXPECT_EQ(uri.GetUserinfo(), "");
  EXPECT_EQ(uri.GetUserinfoEncoded(), "");
  EXPECT_EQ(uri.GetHost(), "");
  EXPECT_EQ(uri.GetHostEncoded(), "");
  EXPECT_EQ(uri.GetPort(), -1);
  EXPECT_TRUE(uri.GetPath().empty());
  EXPECT_TRUE(uri.GetPathEncoded().empty());
  EXPECT_TRUE(uri.GetQuery().empty());
  EXPECT_TRUE(uri.GetQueryEncoded().empty());
  EXPECT_EQ(uri.GetFragment(), "");
  EXPECT_EQ(uri.GetFragmentEncoded(), "");
}

TEST(UriTest, SchemeIsCaseInsensitive) {
  Uri uri;
  uri.SetScheme("ExAmplE+SchemA-X");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetScheme(), "example+schema-x");
}

TEST(UriTest, HostIsCaseInsensitive) {
  Uri uri;
  uri.SetHost("ExAmplE.COM");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetHost(), "example.com");
  EXPECT_EQ(uri.GetHostEncoded(), "example.com");
}

TEST(UriTest, EncodingInHostComponent) {
  Uri uri;

  uri.SetHost("new.EX%41MPLE.COM");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetHost(), "new.ex%41mple.com");
  EXPECT_EQ(uri.GetHostEncoded(),
            "new.ex%2541mple.com");  // %-character was escaped

  uri.SetHostEncoded("new.EX%41MPLE.COM");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetHost(), "new.example.com");
  EXPECT_EQ(uri.GetHostEncoded(), "new.example.com");

  uri.SetHost("ExAmPlE._!_@_#_$_%_^_");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetHost(), "example._!_@_#_$_%_^_");
  EXPECT_EQ(uri.GetHostEncoded(), "example._!_%40_%23_$_%25_%5E_");

  uri.SetHostEncoded("ExAmPlE._!_@_#_$_%25_^_._%21_%40_%23_%24_%25_%5E_");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  EXPECT_EQ(uri.GetHost(), "example._!_@_#_$_%_^_._!_@_#_$_%_^_");
  EXPECT_EQ(uri.GetHostEncoded(),
            "example._!_%40_%23_$_%25_%5E_._!_%40_%23_$_%25_%5E_");
}

TEST(UriTest, SetPortFromString) {
  Uri uri1;
  Uri uri2;

  EXPECT_TRUE(uri1.SetPort(1234));
  EXPECT_TRUE(uri2.SetPort("1234"));
  EXPECT_EQ(uri1, uri2);

  // -1 and empty string mean "unspecified port".
  EXPECT_TRUE(uri1.SetPort(-1));
  EXPECT_TRUE(uri2.SetPort(""));
  EXPECT_EQ(uri1, uri2);

  EXPECT_FALSE(uri2.SetPort("65536"));
  EXPECT_FALSE(uri2.SetPort("-2"));
  EXPECT_FALSE(uri2.SetPort("+2"));
  EXPECT_FALSE(uri2.SetPort(" 2133"));
  EXPECT_FALSE(uri2.SetPort("0x123"));
}

TEST(UriTest, UriWithAllPrintableASCII) {
  Uri uri;
  std::string host = kPrintableASCII;
  const std::vector<std::string> path = {kPrintableASCII};
  std::vector<std::pair<std::string, std::string>> query = {
      {kPrintableASCII, kPrintableASCII}};

  uri.SetUserinfo(kPrintableASCII);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetHost(host);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetPath(path);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetQuery(query);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  uri.SetFragment(kPrintableASCII);
  ASSERT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);

  // Host is case-insensitive, uppercase letters are normalized to lowercase.
  base::ranges::transform(host, host.begin(), &base::ToLowerASCII<char>);

  EXPECT_EQ(uri.GetUserinfo(), kPrintableASCII);
  EXPECT_EQ(uri.GetHost(), host);
  EXPECT_EQ(uri.GetPath(), path);
  EXPECT_EQ(uri.GetQuery(), query);
  EXPECT_EQ(uri.GetFragment(), kPrintableASCII);
}

TEST(UriTest, BuildingHttpUriWithQuery) {
  UriComponents components("http", "", "example.com", 1234);
  components.query = {{"par1", "val1"}, {"par2", ""}, {"par3", "val3"}};
  TestBuilder(components, "http://example.com:1234?par1=val1&par2&par3=val3");
}

TEST(UriTest, BuildingUriWithAllComponents) {
  UriComponents components("A", "B", "C", 1);
  components.path = {"D", "E"};
  components.query = {{"F", "G"}, {"H", "I"}};
  components.fragment = "J";
  TestBuilder(components, "a://B@c:1/D/E?F=G&H=I#J");
}

TEST(UriTest, BuildingUriWithoutAuthority) {
  UriComponents components("A+1-b.C", "", "", -1);
  components.path = {"//", " "};
  components.fragment = "?#@/";
  TestBuilder(components, "a+1-b.c:/%2F%2F/%20#?%23@/");
}

// Special path segments "." and ".." are reduced when possible.
TEST(UriTest, ParsingOfUriWithReduciblePath) {
  const std::string input_uri =
      "hTTp://exAmple.c%4Fm:234"
      "/very/../../long/.././pAth?parAm=vAlue#?%3f?";
  UriComponents components("http", "", "example.com", 234);
  components.path = {"..", "pAth"};
  components.query = {{"parAm", "vAlue"}};
  components.fragment = "???";
  TestParser(input_uri, components);
}

TEST(UriTest, ParsingOfUriWithoutPort) {
  // When a Port is not specified and the Scheme has a default port number,
  // the default port number is set.
  TestParser("hTTp://exAmple.com",
             UriComponents("http", "", "example.com", 80));
  // When the Scheme does not have a default port number, the value of Port
  // remains "unspecified".
  TestParser("X-x://exAmple.com", UriComponents("x-x", "", "example.com"));
}

TEST(UriTest, ParsingOfUriWithUTF8Characters) {
  // On the input, bytes defining UTF-8 characters can be %-escaped or
  // specified directly.
  const std::string uri =
      "http://utf8.test?"
      "zażółć=za%c5%bc%c3%b3%c5%82%c4%87&"
      "gęślą=\x67\xC4\x99\xC5%9B%6C%C4\x85&"
      "jaźń=ja%c5%ba%c5%84";
  UriComponents components("http", "", "utf8.test", 80);
  components.query = {
      {"zażółć", "zażółć"}, {"gęślą", "gęślą"}, {"jaźń", "jaźń"}};
  TestParser(uri, components);
}

// Leading and trailing whitespaces are ignored.
TEST(UriTest, ParsingOfUriWithLeadingAndTrailingWhitespaces) {
  const std::string uri = " \t\n\r\f\vSC://WITH.whitespaces# END \t\n\r\f\v";
  UriComponents components("sc", "", "with.whitespaces");
  components.fragment = " END";
  TestParser(uri, components);
}

// Empty components are accepted.
TEST(UriTest, NormalizationOfEmptyUri) {
  TestNormalization("://@:/?#", "");
}

TEST(UriTest, NormalizationOfUriWithoutAuthority) {
  // When Userinfo, Host and Port are not specified, the "//" prefix is
  // skipped.
  TestNormalization("xx://@:/my/path?#fragment", "xx:/my/path#fragment");
  TestNormalization("xx:///my/path?#fragment", "xx:/my/path#fragment");
  // The same happens when the Port number is equal to the default port number
  // of the Scheme.
  TestNormalization("http://:80/my/path?#fragment", "http:/my/path#fragment");
}

// In the normalized URI, all bytes being part of UTF-8 characters must be
// %-escaped.
TEST(UriTest, NormalizationOfUriWithUTF8Characters) {
  const std::string uri =
      "http://utf8.test?"
      "zażółć=za%c5%bc%c3%b3%c5%82%c4%87&"
      "gęślą=\x67\xC4\x99\xC5%9B%6C%C4\x85&"
      "jaźń=ja%c5%ba%c5%84";
  const std::string uri_normalized =
      "http://utf8.test?"
      "za%C5%BC%C3%B3%C5%82%C4%87=za%C5%BC%C3%B3%C5%82%C4%87&"
      "g%C4%99%C5%9Bl%C4%85=g%C4%99%C5%9Bl%C4%85&"
      "ja%C5%BA%C5%84=ja%C5%BA%C5%84";
  TestNormalization(uri, uri_normalized);
}

TEST(UriTest, ParserErrorDisallowedASCIICharacter) {
  // Non-printable character (0xFF) inside the Host component.
  Uri uri(" \t\n\r\f\vHTTP://BAD.\xff.CHaracter# \t\n\r\f\v");
  const Uri::ParserError pe = uri.GetLastParsingError();
  EXPECT_EQ(pe.status, Uri::ParserStatus::kDisallowedASCIICharacter);
  EXPECT_EQ(pe.parsed_chars, 17u);
  EXPECT_EQ(pe.parsed_strings, 0u);
}

TEST(UriTest, ParserErrorInvalidPercentEncoding) {
  Uri uri;
  // The first percent character has no following ASCII code.
  uri.SetHostEncoded("ExAmPlE._!_@_#_$_%_^_._%21_%40_%23_%24_%25_%5E_");
  EXPECT_EQ(uri.GetLastParsingError().status,
            Uri::ParserStatus::kInvalidPercentEncoding);
  EXPECT_EQ(uri.GetLastParsingError().parsed_chars, 17u);
}

TEST(UriTest, ParserErrorInvalidUTF8Character) {
  // Broken UTF-8 character in the Path (the byte after 0xC5 is wrong).
  Uri uri("http://host/utf8_\xC5\x3C_is_broken");
  const Uri::ParserError pe = uri.GetLastParsingError();
  EXPECT_EQ(pe.status, Uri::ParserStatus::kInvalidUTF8Character);
  EXPECT_EQ(pe.parsed_chars, 18u);
  EXPECT_EQ(pe.parsed_strings, 0u);
}

// Parameters in Query cannot have empty names.
TEST(UriTest, ParserErrorEmptyParameterNameInQuery) {
  Uri uri;
  std::vector<std::pair<std::string, std::string>> query;
  query = {{"name1", "value1"}, {"", "value2"}};
  EXPECT_FALSE(uri.SetQuery(query));
  const Uri::ParserError pe1 = uri.GetLastParsingError();
  EXPECT_EQ(pe1.status, Uri::ParserStatus::kEmptyParameterNameInQuery);
  EXPECT_EQ(pe1.parsed_chars, 0u);
  EXPECT_EQ(pe1.parsed_strings, 2u);
}

// Port number cannot have non-digit characters.
TEST(UriTest, ParserErrorInvalidPortNumber) {
  Uri uri("http://my.weird.port.number:+123");
  const Uri::ParserError pe = uri.GetLastParsingError();
  EXPECT_EQ(pe.status, Uri::ParserStatus::kInvalidPortNumber);
  EXPECT_EQ(pe.parsed_chars, 28u);
  EXPECT_EQ(pe.parsed_strings, 0u);
}

// Path cannot have empty segments.
TEST(UriTest, ParserErrorEmptySegmentInPath) {
  Uri uri;
  EXPECT_FALSE(uri.SetPathEncoded("/segment1//segment3"));
  const Uri::ParserError pe2 = uri.GetLastParsingError();
  EXPECT_EQ(pe2.status, Uri::ParserStatus::kEmptySegmentInPath);
  EXPECT_EQ(pe2.parsed_chars, 10u);
  EXPECT_EQ(pe2.parsed_strings, 0u);
}

TEST(UriTest, ParserErrorInPath) {
  // Non-printable character (0xBA) inside the path.
  Uri uri(
      "  HTTP://example.org/aa/\xba_d/cc"
      "?name1&name2=param2&\xba_d=character#here\xba ");
  const Uri::ParserError pe = uri.GetLastParsingError();
  EXPECT_EQ(pe.status, Uri::ParserStatus::kDisallowedASCIICharacter);
  EXPECT_EQ(pe.parsed_chars, 24u);
  EXPECT_EQ(pe.parsed_strings, 0u);
}

TEST(UriTest, ParserErrorInQuery) {
  // Non-printable character (0xBA) inside the query.
  Uri uri(
      "  HTTP://example.org/aa/bb/cc"
      "?name1&name2=param2&\xba_d=character#here\xba ");
  const Uri::ParserError pe = uri.GetLastParsingError();
  EXPECT_EQ(pe.status, Uri::ParserStatus::kDisallowedASCIICharacter);
  EXPECT_EQ(pe.parsed_chars, 49u);
  EXPECT_EQ(pe.parsed_strings, 0u);
}

TEST(UriTest, ParserErrorInFragment) {
  // Non-printable character (0xBA) inside the fragment.
  Uri uri(
      "  HTTP://example.org/aa/bb/cc"
      "?name1&name2=param2&good=character#here\xba ");
  const Uri::ParserError pe = uri.GetLastParsingError();
  EXPECT_EQ(pe.status, Uri::ParserStatus::kDisallowedASCIICharacter);
  EXPECT_EQ(pe.parsed_chars, 68u);
  EXPECT_EQ(pe.parsed_strings, 0u);
}

TEST(UriTest, GetQueryAsMap) {
  Uri uri("ipp://example.org?p1&p2=val&p1=123&p3=aa&p1=&p2=val&other=x&end");
  EXPECT_EQ(uri.GetLastParsingError().status, Uri::ParserStatus::kNoErrors);
  using KeyValue = std::pair<std::string, std::vector<std::string>>;
  // Parameters from the query sorted by keys.
  const KeyValue e1("end", {""});
  const KeyValue e2("other", {"x"});
  const KeyValue e3("p1", {"", "123", ""});
  const KeyValue e4("p2", {"val", "val"});
  const KeyValue e5("p3", {"aa"});
  EXPECT_THAT(uri.GetQueryAsMap(), testing::ElementsAre(e1, e2, e3, e4, e5));
}

}  // namespace
}  // namespace chromeos
