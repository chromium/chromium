// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef ContentSettingsPattern::BuilderInterface BuilderInterface;
}  // namespace

class MockBuilder : public ContentSettingsPattern::BuilderInterface {
 public:
  MOCK_METHOD0(WithSchemeWildcard, BuilderInterface*());
  MOCK_METHOD0(WithDomainWildcard, BuilderInterface*());
  MOCK_METHOD0(WithPortWildcard, BuilderInterface*());
  MOCK_METHOD1(WithScheme, BuilderInterface*(const std::string& scheme));
  MOCK_METHOD1(WithHost, BuilderInterface*(const std::string& host));
  MOCK_METHOD1(WithPort, BuilderInterface*(const std::string& port));
  MOCK_METHOD1(WithPath, BuilderInterface*(const std::string& path));
  MOCK_METHOD0(WithPathWildcard, BuilderInterface*());
  MOCK_METHOD0(Invalid, BuilderInterface*());
  MOCK_METHOD0(Build, ContentSettingsPattern());
};

TEST(ContentSettingsPatternParserTest, ParsePatterns) {
  // Test valid patterns
  ::testing::StrictMock<MockBuilder> builder;

  // WithPathWildcard() is not called for "*". (Need a strict Mock for this
  // case.)
  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.youtube.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://www.youtube.com:8080",
                                         &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.gmail.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("80"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("*://www.gmail.com:80", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.gmail.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://www.gmail.com:*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard())
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("google.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("80"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://[*.]google.com:80", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("https"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("[::1]"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("https://[::1]:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("127.0.0.1"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://127.0.0.1:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Test valid pattern short forms
  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.youtube.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("www.youtube.com:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.youtube.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("www.youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("youtube.com"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("[*.]youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Test invalid patterns
  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("*youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("*.youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("www.youtube.com*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Tests for superfluous dot after domain wildcard
  {
    auto real_builder = ContentSettingsPattern::CreateBuilder();
    content_settings::PatternParser::Parse("[*.].youtube.com",
                                           real_builder.get());
    EXPECT_FALSE(real_builder->Build().IsValid());
  }
  {
    auto real_builder = ContentSettingsPattern::CreateBuilder();
    content_settings::PatternParser::Parse("[*.]%2Eyoutube.com",
                                           real_builder.get());
    EXPECT_FALSE(real_builder->Build().IsValid());
  }
}

TEST(ContentSettingsPatternParserTest, ParseFilePatterns) {
  ::testing::StrictMock<MockBuilder> builder;

  EXPECT_CALL(builder, WithScheme("file"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/foo/bar/test.html"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file:///foo/bar/test.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file://*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file://*/", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPathWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file://*/*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPathWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file:///*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Invalid file patterns.
  EXPECT_CALL(builder, WithScheme("file"))
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid()).WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file://**", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file"))
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("foo")).WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid()).WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file://foo:123", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file"))
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("foo")).WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid()).WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("file://foo:*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);
}

TEST(ContentSettingsPatternParserTest, ParseChromePatterns) {
  // The schemes chrome-extension://, chrome-search:// and isolated-app:// are
  // valid, and chrome-not-search:// is not, because the former three are
  // registered as non-domain wildcard non-port schemes in
  // components_test_suite.cc, and the last one isn't.
  ::testing::StrictMock<MockBuilder> builder;

  // Valid chrome-extension:// URL.
  EXPECT_CALL(builder, WithScheme("chrome-extension"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("peoadpeiejnhkmpaakpnompolbglelel"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Valid chrome-search:// URL.
  EXPECT_CALL(builder, WithScheme("chrome-search"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("local-ntp"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/local-ntp.html"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "chrome-search://local-ntp/local-ntp.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Valid isolated-app:// URL.
  EXPECT_CALL(builder, WithScheme("isolated-app"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "isolated-app://pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Not a non-domain wildcard non-port scheme implies a port is parsed.
  EXPECT_CALL(builder, WithScheme("chrome-not-search"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("local-ntp"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/local-ntp.html"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "chrome-not-search://local-ntp/local-ntp.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);
}

TEST(ContentSettingsPatternParserTest,
     ParseInvalidNonDomainWildcardNonPortPatterns) {
  ::testing::StrictMock<MockBuilder> builder;

  // Domain wildcard for scheme without domain wildcards.
  EXPECT_CALL(builder, WithScheme("chrome-search"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse("chrome-search://*/local-ntp.html",
                                         &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Domain wildcard for scheme without domain wildcards.
  EXPECT_CALL(builder, WithScheme("chrome-search"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "chrome-search://*local-ntp/local-ntp.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Port for scheme without ports.
  EXPECT_CALL(builder, WithScheme("chrome-search"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("local-ntp"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "chrome-search://local-ntp:65535/local-ntp.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Port wildcard for scheme without ports.
  EXPECT_CALL(builder, WithScheme("chrome-search"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("local-ntp"))
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  EXPECT_CALL(builder, Invalid())
      .Times(1)
      .WillOnce(::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "chrome-search://local-ntp:*/local-ntp.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);
}

TEST(ContentSettingsPatternParserTest, SerializePatterns) {
  ContentSettingsPattern::PatternParts parts;
  parts.scheme = "http";
  parts.host = "www.youtube.com";
  parts.port = "8080";
  EXPECT_STREQ("http://www.youtube.com:8080",
               content_settings::PatternParser::ToString(parts).c_str());

  parts = ContentSettingsPattern::PatternParts();
  parts.scheme = "file";
  parts.path = "/foo/bar/test.html";
  EXPECT_STREQ("file:///foo/bar/test.html",
               content_settings::PatternParser::ToString(parts).c_str());

  parts = ContentSettingsPattern::PatternParts();
  parts.scheme = "file";
  parts.path = "";
  parts.is_path_wildcard = true;
  EXPECT_EQ("file:///*", content_settings::PatternParser::ToString(parts));

  parts = ContentSettingsPattern::PatternParts();
  parts.scheme = "chrome-search";
  parts.host = "local-ntp";
  EXPECT_EQ("chrome-search://local-ntp/",
            content_settings::PatternParser::ToString(parts));

  parts = ContentSettingsPattern::PatternParts();
  parts.scheme = "chrome-extension";
  parts.host = "peoadpeiejnhkmpaakpnompolbglelel";
  EXPECT_EQ("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/",
            content_settings::PatternParser::ToString(parts));
}

TEST(ContentSettingsPatternParserTest, IdempotencyOfCanonicalization) {
  const std::string pattern_specs[] = {
      "abc",
      "https://chromium.org",
      "file:///foo/",
      "file:///foo/:/bar/:/baz",
      "https://foo/:/bar/:/baz",
      "file://:/path",
      "file://:/:",  // crbug.com/1196591
      "file:///C:/Users/a.txt",
      "filE:///foo/",  // crbug.com/1323130
  };

  for (const std::string& spec : pattern_specs) {
    SCOPED_TRACE("spec: " + spec);
    auto builder = ContentSettingsPattern::CreateBuilder();
    content_settings::PatternParser::Parse(spec, builder.get());
    ContentSettingsPattern pattern = builder->Build();
    EXPECT_TRUE(pattern.IsValid());
    std::string canonical = pattern.ToString();

    auto builder2 = ContentSettingsPattern::CreateBuilder();
    content_settings::PatternParser::Parse(canonical, builder2.get());
    ContentSettingsPattern pattern2 = builder2->Build();
    EXPECT_TRUE(pattern2.IsValid());
    std::string canonical2 = pattern2.ToString();

    EXPECT_EQ(canonical, canonical2);
    EXPECT_EQ(pattern.Compare(pattern2),
              ContentSettingsPattern::Relation::IDENTITY);
  }
}
