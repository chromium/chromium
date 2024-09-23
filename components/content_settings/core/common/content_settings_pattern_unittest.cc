// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_pattern.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

ContentSettingsPattern Pattern(const std::string& str) {
  return ContentSettingsPattern::FromString(str);
}

}  // namespace

TEST(ContentSettingsPatternTest, RealWorldPatterns) {
  // This is the place for real world patterns that unveiled bugs.
  EXPECT_STREQ("[*.]ikea.com", Pattern("[*.]ikea.com").ToString().c_str());
}

TEST(ContentSettingsPatternTest, GURL) {
  // Document and verify GURL behavior.
  GURL url("http://mail.google.com:80");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("http://mail.google.com");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("https://mail.google.com:443");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("https://mail.google.com");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("http://mail.google.com");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());
}

TEST(ContentSettingsPatternTest, FromURL) {
  // NOTICE: When content settings pattern are created from a GURL the following
  // happens:
  // - If the GURL scheme is "http" the scheme wildcard is used. Otherwise the
  //   GURL scheme is used.
  // - A domain wildcard is added to the GURL host.
  // - A port wildcard is used instead of the schemes default port.
  //   In case of non-default ports the specific GURL port is used.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("[*.]www.youtube.com", pattern.ToString().c_str());

  // Patterns created from a URL.
  pattern = ContentSettingsPattern::FromURL(GURL("http://www.google.com"));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("http://foo.www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:80")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:81")));
  EXPECT_FALSE(pattern.Matches(GURL("https://mail.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.google.com")));

  pattern = ContentSettingsPattern::FromURL(GURL("http://www.google.com:80"));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:80")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:81")));

  pattern = ContentSettingsPattern::FromURL(GURL("https://www.google.com:443"));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://foo.www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.google.com:443")));
  EXPECT_FALSE(pattern.Matches(GURL("https://www.google.com:444")));
  EXPECT_FALSE(pattern.Matches(GURL("http://www.google.com:443")));

  pattern = ContentSettingsPattern::FromURL(GURL("https://127.0.0.1"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("https://127.0.0.1:443", pattern.ToString().c_str());

  pattern = ContentSettingsPattern::FromURL(GURL("http://[::1]"));
  EXPECT_TRUE(pattern.IsValid());

  pattern = ContentSettingsPattern::FromURL(GURL("file:///foo/bar.html"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_EQ("file:///foo/bar.html", pattern.ToString());

  // WebUI and other portless schemes shouldn't use domain wildcards.
  pattern = ContentSettingsPattern::FromURL(GURL("chrome://test"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_FALSE(pattern.Matches(GURL("chrome://foo.test")));
  pattern = ContentSettingsPattern::FromURL(GURL("chrome-untrusted://test"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_FALSE(pattern.Matches(GURL("chrome-untrusted://foo.test")));
  pattern = ContentSettingsPattern::FromURL(GURL("devtools://devtools"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_FALSE(pattern.Matches(GURL("devtools://foo.devtools")));

  // Ports should be ignored for portless schemes.
  pattern = ContentSettingsPattern::FromURL(GURL("devtools://devtools"));
  EXPECT_TRUE(pattern.Matches(GURL("devtools://devtools:80")));
  EXPECT_TRUE(pattern.Matches(GURL("devtools://devtools:81")));

  // TODO(crbug.com/40252232): Including a port with a portless scheme should
  // return an invalid pattern.
  pattern = ContentSettingsPattern::FromURL(GURL("devtools://devtools:80"));
  EXPECT_TRUE(pattern.Matches(GURL("devtools://devtools:80")));
  EXPECT_TRUE(pattern.Matches(GURL("devtools://devtools:81")));

  // Unknown schemes shouldn't create valid ContentSettingsPatterns.
  pattern = ContentSettingsPattern::FromURL(GURL("invalid://test"));
  EXPECT_FALSE(pattern.IsValid());
}

TEST(ContentSettingsPatternTest, FilesystemUrls) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.google.com"));
  EXPECT_TRUE(
      pattern.Matches(GURL("filesystem:http://www.google.com/temporary/")));
  EXPECT_TRUE(
      pattern.Matches(GURL("filesystem:http://foo.www.google.com/temporary/")));
  EXPECT_TRUE(
      pattern.Matches(GURL("filesystem:http://www.google.com:80/temporary/")));
  EXPECT_TRUE(
      pattern.Matches(GURL("filesystem:http://www.google.com:81/temporary/")));

  pattern = ContentSettingsPattern::FromURL(GURL("https://www.google.com"));
  EXPECT_TRUE(
      pattern.Matches(GURL("filesystem:https://www.google.com/temporary/")));
  EXPECT_TRUE(pattern.Matches(
      GURL("filesystem:https://www.google.com:443/temporary/")));
  EXPECT_TRUE(pattern.Matches(
      GURL("filesystem:https://foo.www.google.com/temporary/")));
  EXPECT_FALSE(
      pattern.Matches(GURL("filesystem:https://www.google.com:81/temporary/")));

  // A pattern from a filesystem URLs is equivalent to a pattern from the inner
  // URL of the filesystem URL.
  ContentSettingsPattern pattern2 = ContentSettingsPattern::FromURL(
      GURL("filesystem:https://www.google.com/temporary/"));
  EXPECT_EQ(ContentSettingsPattern::IDENTITY, pattern.Compare(pattern2));

  EXPECT_STREQ("https://[*.]www.google.com:443", pattern2.ToString().c_str());

  // TODO(msramek): Filesystem URLs do not return correct paths. For example,
  // GURL("filesystem:file:///temporary/test.txt").inner_url().path() returns
  // only '/temporary' instead of 'temporary/test.txt'. crbug.com/568110.
  pattern = ContentSettingsPattern::FromURL(
      GURL("filesystem:file:///temporary/foo/bar"));
  EXPECT_TRUE(pattern.Matches(GURL("filesystem:file:///temporary/")));
  EXPECT_TRUE(pattern.Matches(GURL("filesystem:file:///temporary/test.txt")));
  EXPECT_TRUE(pattern.Matches(GURL("file:///temporary")));
  EXPECT_FALSE(pattern.Matches(GURL("file://foo/bar")));
  pattern2 = ContentSettingsPattern::FromURL(
      GURL("filesystem:file:///persistent/foo2/bar2"));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            pattern.Compare(pattern2));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            pattern2.Compare(pattern));
}

TEST(ContentSettingsPatternTest, FromURLNoWildcard) {
  // If no port is specifed GURLs always use the default port for the schemes
  // HTTP and HTTPS. Hence a GURL always carries a port specification either
  // explicitly or implicitly. Therefore if a content settings pattern is
  // created from a GURL with no wildcard, specific values are used for the
  // scheme, host and port part of the pattern.
  // Creating content settings patterns from strings behaves different. Pattern
  // parts that are omitted in pattern specifications (strings), are completed
  // with a wildcard.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("http://www.example.com"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("http://www.example.com:80", pattern.ToString().c_str());
  EXPECT_TRUE(pattern.Matches(GURL("http://www.example.com")));
  EXPECT_FALSE(pattern.Matches(GURL("https://www.example.com")));
  EXPECT_FALSE(pattern.Matches(GURL("http://foo.www.example.com")));

  pattern = ContentSettingsPattern::FromURLNoWildcard(
      GURL("https://www.example.com"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("https://www.example.com:443", pattern.ToString().c_str());
  EXPECT_FALSE(pattern.Matches(GURL("http://www.example.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.example.com")));
  EXPECT_FALSE(pattern.Matches(GURL("http://foo.www.example.com")));

  // Pattern for filesystem URLs
  pattern = ContentSettingsPattern::FromURLNoWildcard(
      GURL("filesystem:http://www.google.com/temporary/"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com")));
  EXPECT_FALSE(pattern.Matches(GURL("http://foo.www.google.com")));
  EXPECT_TRUE(
      pattern.Matches(GURL("filesystem:http://www.google.com/persistent/")));
  EXPECT_FALSE(
      pattern.Matches(GURL("filesystem:https://www.google.com/persistent/")));
  EXPECT_FALSE(
      pattern.Matches(GURL("filesystem:https://www.google.com:81/temporary/")));
  EXPECT_FALSE(pattern.Matches(
      GURL("filesystem:https://foo.www.google.com/temporary/")));

  pattern = ContentSettingsPattern::FromURLNoWildcard(GURL("chrome://test"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_TRUE(pattern.Matches(GURL("chrome://test")));

  pattern = ContentSettingsPattern::FromURLNoWildcard(
      GURL("chrome-untrusted://test"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_TRUE(pattern.Matches(GURL("chrome-untrusted://test")));

  pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("devtools://devtools"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_TRUE(pattern.Matches(GURL("devtools://devtools")));
}

TEST(ContentSettingsPatternTest, URLToSchemefulSitePattern) {
  // Only uses the eTLD+1 (aka registrable domain)
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://mail.google.com"))
                .ToString());
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://www.foo.mail.google.com"))
                .ToString());
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://google.com"))
                .ToString());

  // Includes the (right) scheme
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://google.com"))
                .ToString());
  EXPECT_EQ("https://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("https://google.com"))
                .ToString());

  // Strips the port
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://google.com:3000"))
                .ToString());
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://google.com:80"))
                .ToString());
  EXPECT_EQ("https://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("https://google.com:443"))
                .ToString());

  // Strips the path
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://google.com/example/"))
                .ToString());
  EXPECT_EQ("http://[*.]google.com",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("http://google.com/example/example.html"))
                .ToString());

  // Opaque origins shouldn't match anything.
  EXPECT_EQ("", ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    GURL("data:text/html,<body>Hello World</body>"))
                    .ToString());

  // This should mirror SchemefulSite which considers file URLs
  // equal, ignoring the path.
  EXPECT_EQ("file:///*", ContentSettingsPattern::FromURLToSchemefulSitePattern(
                             GURL("file:///foo/bar.html"))
                             .ToString());

  EXPECT_EQ("https://127.0.0.1",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("https://127.0.0.1:8080"))
                .ToString());
  EXPECT_EQ("https://[::1]",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("https://[::1]:8080"))
                .ToString());

  EXPECT_EQ("https://localhost",
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                GURL("https://localhost:3000"))
                .ToString());

  // Invalid patterns
  EXPECT_FALSE(ContentSettingsPattern::FromURLToSchemefulSitePattern(
                   GURL("invalid://test:3000"))
                   .IsValid());
  EXPECT_FALSE(ContentSettingsPattern::FromURLToSchemefulSitePattern(
                   GURL("invalid://test.com/path"))
                   .IsValid());

  // URL patterns that are not currently matched
  EXPECT_EQ("", ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    GURL("filesystem:http://www.google.com/temporary/"))
                    .ToString());
  EXPECT_EQ("", ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    GURL("chrome://test"))
                    .ToString());
  EXPECT_EQ("", ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    GURL("devtools://devtools/"))
                    .ToString());
}

// The static Wildcard() method goes through a fast path and avoids the Builder
// pattern. Ensure that it yields the exact same behavior.
TEST(ContentSettingsPatternTest, ValidWildcardFastPath) {
  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
      ContentSettingsPattern::CreateBuilder();
  builder->WithSchemeWildcard()
      ->WithDomainWildcard()
      ->WithPortWildcard()
      ->WithPathWildcard();
  ContentSettingsPattern built_wildcard = builder->Build();
  EXPECT_EQ(built_wildcard, ContentSettingsPattern::Wildcard());
}

TEST(ContentSettingsPatternTest, Wildcard) {
  EXPECT_TRUE(ContentSettingsPattern::Wildcard().IsValid());

  EXPECT_TRUE(ContentSettingsPattern::Wildcard().Matches(
      GURL("http://www.google.com")));
  EXPECT_TRUE(ContentSettingsPattern::Wildcard().Matches(
      GURL("https://www.google.com")));
  EXPECT_TRUE(
      ContentSettingsPattern::Wildcard().Matches(GURL("https://myhost:8080")));
  EXPECT_TRUE(
      ContentSettingsPattern::Wildcard().Matches(GURL("file:///foo/bar.txt")));

  EXPECT_STREQ("*", ContentSettingsPattern::Wildcard().ToString().c_str());

  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            ContentSettingsPattern::Wildcard().Compare(
                ContentSettingsPattern::Wildcard()));
}

TEST(ContentSettingsPatternTest, TrimEndingDotFromHost) {
  EXPECT_TRUE(Pattern("www.example.com").IsValid());
  EXPECT_TRUE(
      Pattern("www.example.com").Matches(GURL("http://www.example.com")));
  EXPECT_TRUE(
      Pattern("www.example.com").Matches(GURL("http://www.example.com.")));

  EXPECT_TRUE(Pattern("www.example.com.").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("www.example.com.").ToString().c_str());

  EXPECT_TRUE(Pattern("www.example.com.") == Pattern("www.example.com"));
  EXPECT_TRUE(Pattern("www.example.com.") == Pattern("www.example.com."));

  EXPECT_TRUE(Pattern(".").IsValid());
  EXPECT_STREQ(".", Pattern(".").ToString().c_str());
  EXPECT_TRUE(Pattern("http://.").Matches(GURL("http://.")));

  EXPECT_TRUE(Pattern("a..b").IsValid());
  EXPECT_STREQ("a..b", Pattern("a..b").ToString().c_str());
  EXPECT_TRUE(Pattern("a..b").Matches(GURL("http://a..b")));

  EXPECT_TRUE(Pattern("a..b.").IsValid());
  EXPECT_STREQ("a..b", Pattern("a..b.").ToString().c_str());
  EXPECT_TRUE(Pattern("a..b.").Matches(GURL("http://a..b.")));

  EXPECT_FALSE(Pattern("..").IsValid());
  EXPECT_FALSE(Pattern("a..").IsValid());
}

TEST(ContentSettingsPatternTest, FromString_WithNoWildcards) {
  // HTTP patterns with default port.
  EXPECT_TRUE(Pattern("http://www.example.com:80").IsValid());
  EXPECT_STREQ("http://www.example.com:80",
               Pattern("http://www.example.com:80").ToString().c_str());
  // HTTP patterns with none default port.
  EXPECT_TRUE(Pattern("http://www.example.com:81").IsValid());
  EXPECT_STREQ("http://www.example.com:81",
               Pattern("http://www.example.com:81").ToString().c_str());

  // HTTPS patterns with default port.
  EXPECT_TRUE(Pattern("https://www.example.com:443").IsValid());
  EXPECT_STREQ("https://www.example.com:443",
               Pattern("https://www.example.com:443").ToString().c_str());
  // HTTPS patterns with none default port.
  EXPECT_TRUE(Pattern("https://www.example.com:8080").IsValid());
  EXPECT_STREQ("https://www.example.com:8080",
               Pattern("https://www.example.com:8080").ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromString_FilePatterns) {
  // "/" is an invalid file path.
  EXPECT_FALSE(Pattern("file:///").IsValid());

  // Non-empty domains aren't allowed in file patterns.
  EXPECT_FALSE(Pattern("file://foo/").IsValid());
  EXPECT_FALSE(Pattern("file://localhost/foo/bar/test.html").IsValid());
  EXPECT_FALSE(Pattern("file://*").IsValid());
  EXPECT_FALSE(Pattern("file://*/").IsValid());
  EXPECT_FALSE(Pattern("file://*/*").IsValid());
  EXPECT_FALSE(Pattern("file://*/foo/bar/test.html").IsValid());
  EXPECT_FALSE(Pattern("file://[*.]/").IsValid());

  // This is the only valid file path wildcard format.
  EXPECT_TRUE(Pattern("file:///*").IsValid());
  EXPECT_EQ("file:///*", Pattern("file:///*").ToString());

  // It matches every file pattern.
  ContentSettingsPattern file_wildcard = Pattern("file:///*");
  EXPECT_TRUE(file_wildcard.Matches(GURL("file:///tmp/test.html")));
  EXPECT_TRUE(file_wildcard.Matches(GURL("file://localhost/tmp/test.html")));

  // Wildcards are not allowed anywhere in the file path.
  EXPECT_FALSE(Pattern("file:///f*o/bar/file.html").IsValid());
  EXPECT_FALSE(Pattern("file:///*/bar/file.html").IsValid());
  EXPECT_FALSE(Pattern("file:///foo/*").IsValid());
  EXPECT_FALSE(Pattern("file:///foo/bar/*").IsValid());
  EXPECT_FALSE(Pattern("file:///foo/*/file.html").IsValid());
  EXPECT_FALSE(Pattern("file:///foo/bar/*.html").IsValid());
  EXPECT_FALSE(Pattern("file:///foo/bar/file.*").IsValid());

  // File patterns match URLs with the same path on any host.
  EXPECT_TRUE(Pattern("file:///foo/bar/file.html")
                  .Matches(GURL("file://localhost/foo/bar/file.html")));
  EXPECT_TRUE(Pattern("file:///foo/bar/file.html")
                  .Matches(GURL("file://example.com/foo/bar/file.html")));
  EXPECT_FALSE(Pattern("file:///foo/bar/file.html")
                   .Matches(GURL("file://localhost/foo/bar/other.html")));
  EXPECT_FALSE(Pattern("file:///foo/bar/file.html")
                   .Matches(GURL("file://example.com/foo/bar/other.html")));

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("file:///tmp/test.html"));

  EXPECT_TRUE(pattern.IsValid());
  EXPECT_EQ("file:///tmp/test.html", pattern.ToString());
  EXPECT_TRUE(pattern.Matches(GURL("file:///tmp/test.html")));
  EXPECT_FALSE(pattern.Matches(GURL("file:///tmp/other.html")));
  EXPECT_FALSE(pattern.Matches(GURL("http://example.org/")));

  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("file:///tmp/test.html");
  ContentSettingsPattern pattern3 =
      ContentSettingsPattern::FromString("file:///tmp/other.html");

  EXPECT_EQ(ContentSettingsPattern::IDENTITY, pattern.Compare(pattern));
  EXPECT_EQ(ContentSettingsPattern::IDENTITY, pattern.Compare(pattern2));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            pattern.Compare(pattern3));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            pattern3.Compare(pattern));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR, file_wildcard.Compare(pattern));
  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            pattern.Compare(file_wildcard));
}

TEST(ContentSettingsPatternTest, FromString_ExtensionPatterns) {
  EXPECT_TRUE(Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
                  .IsValid());
  EXPECT_EQ("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/",
            Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
                .ToString());
  EXPECT_TRUE(Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
                  .Matches(GURL(
                      "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")));
}

TEST(ContentSettingsPatternTest, FromString_IsolatedAppPatterns) {
  EXPECT_TRUE(
      Pattern("isolated-app://"
              "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/")
          .IsValid());
  EXPECT_EQ(
      "isolated-app://"
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/",
      Pattern("isolated-app://"
              "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/")
          .ToString());
  EXPECT_TRUE(
      Pattern("isolated-app://"
              "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/")
          .Matches(GURL(
              "isolated-app://"
              "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/")));
}

TEST(ContentSettingsPatternTest, FromString_SearchPatterns) {
  EXPECT_TRUE(Pattern("chrome-search://local-ntp/").IsValid());
  EXPECT_EQ("chrome-search://local-ntp/",
            Pattern("chrome-search://local-ntp/").ToString());
  EXPECT_TRUE(Pattern("chrome-search://local-ntp/")
                  .Matches(GURL("chrome-search://local-ntp/")));
}

TEST(ContentSettingsPatternTest, FromString_WithIPAdresses) {
  // IPv4
  EXPECT_TRUE(Pattern("192.168.0.1").IsValid());
  EXPECT_STREQ("192.168.1.1", Pattern("192.168.1.1").ToString().c_str());
  EXPECT_TRUE(Pattern("https://192.168.0.1:8080").IsValid());
  EXPECT_STREQ("https://192.168.0.1:8080",
               Pattern("https://192.168.0.1:8080").ToString().c_str());

  // Subdomain wildcards should be only valid for hosts, not for IP addresses.
  EXPECT_FALSE(Pattern("[*.]127.0.0.1").IsValid());

  // IPv6
  EXPECT_TRUE(Pattern("[::1]").IsValid());
  EXPECT_STREQ("[::1]", Pattern("[::1]").ToString().c_str());
  EXPECT_TRUE(Pattern("https://[::1]:8080").IsValid());
  EXPECT_STREQ("https://[::1]:8080",
               Pattern("https://[::1]:8080").ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromString_WithWildcards) {
  // Creating content settings patterns from strings completes pattern parts
  // that are omitted in pattern specifications (strings) with a wildcard.

  // The wildcard pattern.
  EXPECT_TRUE(Pattern("*").IsValid());
  EXPECT_STREQ("*", Pattern("*").ToString().c_str());
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            Pattern("*").Compare(ContentSettingsPattern::Wildcard()));

  // Patterns with port wildcard.
  EXPECT_TRUE(Pattern("http://example.com:*").IsValid());
  EXPECT_STREQ("http://example.com",
               Pattern("http://example.com:*").ToString().c_str());

  EXPECT_TRUE(Pattern("https://example.com").IsValid());
  EXPECT_STREQ("https://example.com",
               Pattern("https://example.com").ToString().c_str());

  EXPECT_TRUE(Pattern("*://www.google.com.com:8080").IsValid());
  EXPECT_STREQ("www.google.com:8080",
               Pattern("*://www.google.com:8080").ToString().c_str());
  EXPECT_TRUE(Pattern("*://www.google.com:8080")
                  .Matches(GURL("http://www.google.com:8080")));
  EXPECT_TRUE(Pattern("*://www.google.com:8080")
                  .Matches(GURL("https://www.google.com:8080")));
  EXPECT_FALSE(
      Pattern("*://www.google.com").Matches(GURL("file:///foo/bar.html")));

  EXPECT_TRUE(Pattern("www.example.com:8080").IsValid());

  // Patterns with port and scheme wildcard.
  EXPECT_TRUE(Pattern("*://www.example.com:*").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("*://www.example.com:*").ToString().c_str());

  EXPECT_TRUE(Pattern("*://www.example.com").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("*://www.example.com").ToString().c_str());

  EXPECT_TRUE(Pattern("www.example.com:*").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("www.example.com:*").ToString().c_str());

  EXPECT_TRUE(Pattern("www.example.com").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("www.example.com").ToString().c_str());
  EXPECT_TRUE(
      Pattern("www.example.com").Matches(GURL("http://www.example.com/")));
  EXPECT_FALSE(Pattern("example.com").Matches(GURL("http://example.org/")));

  // Patterns with domain wildcard.
  EXPECT_TRUE(Pattern("[*.]example.com").IsValid());
  EXPECT_STREQ("[*.]example.com",
               Pattern("[*.]example.com").ToString().c_str());
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(GURL("http://example.com/")));
  EXPECT_TRUE(
      Pattern("[*.]example.com").Matches(GURL("http://foo.example.com/")));
  EXPECT_FALSE(Pattern("[*.]example.com").Matches(GURL("http://example.org/")));

  EXPECT_TRUE(
      Pattern("[*.]google.com:80").Matches(GURL("http://mail.google.com:80")));
  EXPECT_FALSE(
      Pattern("[*.]google.com:80").Matches(GURL("http://mail.google.com:81")));
  EXPECT_TRUE(
      Pattern("[*.]google.com:80").Matches(GURL("http://www.google.com")));

  EXPECT_TRUE(Pattern("[*.]google.com:8080")
                  .Matches(GURL("http://mail.google.com:8080")));

  EXPECT_TRUE(Pattern("[*.]google.com:443")
                  .Matches(GURL("https://mail.google.com:443")));
  EXPECT_TRUE(
      Pattern("[*.]google.com:443").Matches(GURL("https://www.google.com")));

  EXPECT_TRUE(Pattern("[*.]google.com:4321")
                  .Matches(GURL("https://mail.google.com:4321")));
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(GURL("http://example.com/")));
  EXPECT_TRUE(
      Pattern("[*.]example.com").Matches(GURL("http://www.example.com/")));

  // Patterns with host wildcard
  EXPECT_TRUE(Pattern("[*.]").IsValid());
  EXPECT_TRUE(Pattern("http://*").IsValid());
  EXPECT_TRUE(Pattern("http://[*.]").IsValid());
  EXPECT_EQ(std::string("http://*"), Pattern("http://[*.]").ToString());
  EXPECT_TRUE(Pattern("http://*:8080").IsValid());
  EXPECT_TRUE(Pattern("*://*").IsValid());
  EXPECT_STREQ("*", Pattern("*://*").ToString().c_str());

  EXPECT_FALSE(Pattern("chrome-extension://*").IsValid());
  EXPECT_FALSE(Pattern("isolated-app://*").IsValid());
}

TEST(ContentSettingsPatternTest, FromString_Canonicalized) {
  // UTF-8 patterns.
  EXPECT_TRUE(Pattern("[*.]\xC4\x87ira.com").IsValid());
  EXPECT_STREQ("[*.]xn--ira-ppa.com",
               Pattern("[*.]\xC4\x87ira.com").ToString().c_str());
  EXPECT_TRUE(Pattern("\xC4\x87ira.com").IsValid());
  EXPECT_STREQ("xn--ira-ppa.com",
               Pattern("\xC4\x87ira.com").ToString().c_str());
  EXPECT_TRUE(Pattern("file:///\xC4\x87ira.html").IsValid());
  EXPECT_STREQ("file:///%C4%87ira.html",
               Pattern("file:///\xC4\x87ira.html").ToString().c_str());

  // File path normalization.
  EXPECT_TRUE(Pattern("file:///tmp/bar/../test.html").IsValid());
  EXPECT_STREQ("file:///tmp/test.html",
               Pattern("file:///tmp/bar/../test.html").ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromString_WebUISchemes) {
  const char* patterns[] = {"chrome://test/", "chrome-untrusted://test/",
                            "devtools://devtools/"};

  for (const char* pattern_str : patterns) {
    ContentSettingsPattern pattern = Pattern(pattern_str);
    EXPECT_TRUE(pattern.IsValid());
    EXPECT_EQ(pattern_str, pattern.ToString());
    EXPECT_TRUE(pattern.Matches(GURL(pattern_str)));
  }
}

TEST(ContentSettingsPatternTest, ToDomainWildcardPattern) {
  // Patterns with scheme, port and path.
  ContentSettingsPattern pattern =
      Pattern("https://www.google.com:81/temporary/");
  ContentSettingsPattern domain_wildcard_pattern =
      ContentSettingsPattern::ToDomainWildcardPattern(pattern);
  EXPECT_TRUE(domain_wildcard_pattern.IsValid());
  EXPECT_EQ("[*.]google.com", domain_wildcard_pattern.ToString());

  // Pattern with host only.
  pattern = Pattern("www.example.com");
  domain_wildcard_pattern =
      ContentSettingsPattern::ToDomainWildcardPattern(pattern);
  EXPECT_TRUE(domain_wildcard_pattern.IsValid());
  EXPECT_EQ("[*.]example.com", domain_wildcard_pattern.ToString());

  // Pattern with domain wildcard.
  pattern = Pattern("https://[*.]example.com");
  domain_wildcard_pattern =
      ContentSettingsPattern::ToDomainWildcardPattern(pattern);
  EXPECT_TRUE(domain_wildcard_pattern.IsValid());
  EXPECT_EQ("[*.]example.com", domain_wildcard_pattern.ToString());

  // Pattern is an ip address.
  pattern = Pattern("1.2.3.4");
  domain_wildcard_pattern =
      ContentSettingsPattern::ToDomainWildcardPattern(pattern);
  EXPECT_FALSE(domain_wildcard_pattern.IsValid());
}

TEST(ContentSettingsPatternTest, ToHostOnlyPattern) {
  // Patterns with scheme, port and path.
  ContentSettingsPattern pattern =
      Pattern("https://www.google.com:81/temporary/");
  ContentSettingsPattern host_only_pattern =
      ContentSettingsPattern::ToHostOnlyPattern(pattern);
  EXPECT_TRUE(host_only_pattern.IsValid());
  EXPECT_EQ("www.google.com", host_only_pattern.ToString());

  // Pattern with host only.
  pattern = Pattern("www.example.com");
  host_only_pattern = ContentSettingsPattern::ToHostOnlyPattern(pattern);
  EXPECT_TRUE(host_only_pattern.IsValid());
  EXPECT_EQ("www.example.com", host_only_pattern.ToString());

  // Pattern with domain wildcard.
  pattern = Pattern("https://[*.]example.com");
  host_only_pattern = ContentSettingsPattern::ToHostOnlyPattern(pattern);
  EXPECT_TRUE(host_only_pattern.IsValid());
  EXPECT_EQ("[*.]example.com", host_only_pattern.ToString());

  // Pattern is an ip address.
  pattern = Pattern("1.2.3.4");
  host_only_pattern = ContentSettingsPattern::ToHostOnlyPattern(pattern);
  EXPECT_TRUE(host_only_pattern.IsValid());
  EXPECT_EQ("1.2.3.4", host_only_pattern.ToString());
}

TEST(ContentSettingsPatternTest, HasDomainWildcard) {
  // Pattern with domain wildcard.
  ContentSettingsPattern pattern = Pattern("[*.]example.com");
  EXPECT_TRUE(pattern.HasDomainWildcard());

  // Pattern with host wildcard.
  pattern = Pattern("*");
  EXPECT_FALSE(pattern.HasDomainWildcard());

  // Patterns with scheme, port and path.
  pattern = Pattern("https://www.google.com:81/temporary/");
  EXPECT_FALSE(pattern.HasDomainWildcard());
}

TEST(ContentSettingsPatternTest, InvalidPatterns) {
  // StubObserver expects an empty pattern top be returned as empty string.
  EXPECT_FALSE(ContentSettingsPattern().IsValid());
  EXPECT_STREQ("", ContentSettingsPattern().ToString().c_str());

  // Empty pattern string
  EXPECT_FALSE(Pattern(std::string()).IsValid());
  EXPECT_STREQ("", Pattern(std::string()).ToString().c_str());

  // Pattern strings with invalid scheme part.
  EXPECT_FALSE(Pattern("ftp://myhost.org").IsValid());
  EXPECT_STREQ("", Pattern("ftp://myhost.org").ToString().c_str());

  // Pattern strings with invalid host part.
  EXPECT_FALSE(Pattern("*example.com").IsValid());
  EXPECT_STREQ("", Pattern("*example.com").ToString().c_str());
  EXPECT_FALSE(Pattern("example.*").IsValid());
  EXPECT_STREQ("", Pattern("example.*").ToString().c_str());
  EXPECT_FALSE(Pattern("*\xC4\x87ira.com").IsValid());
  EXPECT_STREQ("", Pattern("*\xC4\x87ira.com").ToString().c_str());
  EXPECT_FALSE(Pattern("\xC4\x87ira.*").IsValid());
  EXPECT_STREQ("", Pattern("\xC4\x87ira.*").ToString().c_str());

  // Pattern strings with invalid port parts.
  EXPECT_FALSE(Pattern("example.com:abc").IsValid());
  EXPECT_STREQ("", Pattern("example.com:abc").ToString().c_str());

  // Invalid file pattern strings.
  EXPECT_FALSE(Pattern("file://").IsValid());
  EXPECT_STREQ("", Pattern("file://").ToString().c_str());

  // Host having multiple ending dots.
  EXPECT_FALSE(Pattern("www.example.com..").IsValid());
  EXPECT_STREQ("", Pattern("www.example.com..").ToString().c_str());
}

TEST(ContentSettingsPatternTest, UnequalOperator) {
  EXPECT_TRUE(Pattern("http://www.foo.com") != Pattern("http://www.foo.com*"));
  EXPECT_TRUE(Pattern("http://www.foo.com*") !=
              ContentSettingsPattern::Wildcard());

  EXPECT_TRUE(Pattern("http://www.foo.com") !=
              ContentSettingsPattern::Wildcard());

  EXPECT_TRUE(Pattern("http://www.foo.com") != Pattern("www.foo.com"));
  EXPECT_TRUE(Pattern("http://www.foo.com") !=
              Pattern("http://www.foo.com:80"));

  EXPECT_FALSE(Pattern("http://www.foo.com") != Pattern("http://www.foo.com"));
  EXPECT_TRUE(Pattern("http://www.foo.com") == Pattern("http://www.foo.com"));
}

TEST(ContentSettingsPatternTest, Compare) {
  // Test identical patterns patterns.
  ContentSettingsPattern pattern1 = Pattern("http://www.google.com");
  EXPECT_EQ(ContentSettingsPattern::IDENTITY, pattern1.Compare(pattern1));
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            Pattern("http://www.google.com:80")
                .Compare(Pattern("http://www.google.com:80")));
  EXPECT_EQ(
      ContentSettingsPattern::IDENTITY,
      Pattern("*://[*.]google.com:*").Compare(Pattern("*://[*.]google.com:*")));

  ContentSettingsPattern invalid_pattern1;
  ContentSettingsPattern invalid_pattern2 =
      ContentSettingsPattern::FromString("google.com*");

  // Compare invalid patterns.
  EXPECT_TRUE(!invalid_pattern1.IsValid());
  EXPECT_TRUE(!invalid_pattern2.IsValid());
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            invalid_pattern1.Compare(invalid_pattern2));
  EXPECT_TRUE(invalid_pattern1 == invalid_pattern2);

  // Compare a pattern with an IPv4 addresse to a pattern with a domain name.
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("http://www.google.com").Compare(Pattern("127.0.0.1")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("127.0.0.1").Compare(Pattern("http://www.google.com")));
  EXPECT_TRUE(Pattern("127.0.0.1") > Pattern("http://www.google.com"));
  EXPECT_TRUE(Pattern("http://www.google.com") < Pattern("127.0.0.1"));

  // Compare a pattern with an IPv6 address to a patterns with a domain name.
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("http://www.google.com").Compare(Pattern("[::1]")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("[::1]").Compare(Pattern("http://www.google.com")));
  EXPECT_TRUE(Pattern("[::1]") > Pattern("http://www.google.com"));
  EXPECT_TRUE(Pattern("http://www.google.com") < Pattern("[::1]"));

  // Compare a pattern with an IPv6 addresse to a pattern with an IPv4 addresse.
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("127.0.0.1").Compare(Pattern("[::1]")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("[::1]").Compare(Pattern("127.0.0.1")));
  EXPECT_TRUE(Pattern("[::1]") < Pattern("127.0.0.1"));
  EXPECT_TRUE(Pattern("127.0.0.1") > Pattern("[::1]"));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("http://www.google.com")
                .Compare(Pattern("http://www.youtube.com")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("http://[*.]google.com")
                .Compare(Pattern("http://[*.]youtube.com")));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("http://[*.]host.com")
                .Compare(Pattern("http://[*.]evilhost.com")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("*://www.google.com:80")
                .Compare(Pattern("*://www.google.com:8080")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://www.google.com:80")
                .Compare(Pattern("http://www.google.com:80")));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("http://[*.]google.com:90")
                .Compare(Pattern("http://mail.google.com:80")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://[*.]google.com:80")
                .Compare(Pattern("http://mail.google.com:80")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://mail.google.com:*")
                .Compare(Pattern("http://mail.google.com:80")));

  // Test patterns with different precedences.
  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("mail.google.com").Compare(Pattern("[*.]google.com")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("[*.]google.com").Compare(Pattern("mail.google.com")));
  EXPECT_TRUE(Pattern("mail.google.com") > Pattern("[*.]google.com"));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("[*.]mail.google.com").Compare(Pattern("[*.]google.com")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("[*.]google.com").Compare(Pattern("[*.]mail.google.com")));
  EXPECT_TRUE(Pattern("[*.]mail.google.com") > Pattern("[*.]google.com"));

  EXPECT_EQ(
      ContentSettingsPattern::PREDECESSOR,
      Pattern("mail.google.com:80").Compare(Pattern("mail.google.com:*")));
  EXPECT_EQ(
      ContentSettingsPattern::SUCCESSOR,
      Pattern("mail.google.com:*").Compare(Pattern("mail.google.com:80")));
  EXPECT_TRUE(Pattern("mail.google.com:80") > Pattern("mail.google.com:*"));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("https://mail.google.com:*")
                .Compare(Pattern("*://mail.google.com:*")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("*://mail.google.com:*")
                .Compare(Pattern("https://mail.google.com:*")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("*://mail.google.com:80")
                .Compare(Pattern("https://mail.google.com:*")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("https://mail.google.com:*")
                .Compare(Pattern("*://mail.google.com:80")));
}

TEST(ContentSettingsPatternTest, CompareSubdomains) {
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            Pattern("https://[*.]a.b").Compare(Pattern("https://[*.]a.b")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("https://[*.]b.a.a.a").Compare(Pattern("https://[*.]a.a")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("https://[*.]a.a").Compare(Pattern("https://[*.]b.a.a.a")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("https://[*.]a.b.a.b").Compare(Pattern("https://[*.]a.b")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("https://[*.]a.b").Compare(Pattern("https://[*.]a.b.a.b")));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://[*.]a.a").Compare(Pattern("https://[*.]b.a.a.b")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("https://[*.]b.a.a.b").Compare(Pattern("https://[*.]a.a")));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://[*.]a.b").Compare(Pattern("https://[*.]aa.b")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("https://[*.]aa.b").Compare(Pattern("https://[*.]a.b")));
}

TEST(ContentSettingsPatternTest, CompareWithWildcard) {
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            ContentSettingsPattern::Wildcard().Compare(
                ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            ContentSettingsPattern::Wildcard().Compare(Pattern("*")));

  EXPECT_EQ(
      ContentSettingsPattern::PREDECESSOR,
      Pattern("[*.]google.com").Compare(ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("[*.]google.com").Compare(Pattern("*")));

  EXPECT_EQ(
      ContentSettingsPattern::SUCCESSOR,
      ContentSettingsPattern::Wildcard().Compare(Pattern("[*.]google.com")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("*").Compare(Pattern("[*.]google.com")));

  EXPECT_EQ(
      ContentSettingsPattern::PREDECESSOR,
      Pattern("mail.google.com").Compare(ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("mail.google.com").Compare(Pattern("*")));

  EXPECT_EQ(
      ContentSettingsPattern::SUCCESSOR,
      ContentSettingsPattern::Wildcard().Compare(Pattern("mail.google.com")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("*").Compare(Pattern("mail.google.com")));
}

// Legacy tests to ensure backwards compatibility.

TEST(ContentSettingsPatternTest, PatternSupport_Legacy) {
  EXPECT_TRUE(Pattern("[*.]example.com").IsValid());
  EXPECT_TRUE(Pattern("example.com").IsValid());
  EXPECT_TRUE(Pattern("192.168.0.1").IsValid());
  EXPECT_TRUE(Pattern("[::1]").IsValid());
  EXPECT_TRUE(Pattern("file:///tmp/test.html").IsValid());
  EXPECT_FALSE(Pattern("*example.com").IsValid());
  EXPECT_FALSE(Pattern("example.*").IsValid());

  EXPECT_TRUE(Pattern("http://example.com").IsValid());
  EXPECT_TRUE(Pattern("https://example.com").IsValid());

  EXPECT_TRUE(Pattern("[*.]example.com").Matches(GURL("http://example.com/")));
  EXPECT_TRUE(
      Pattern("[*.]example.com").Matches(GURL("http://www.example.com/")));
  EXPECT_TRUE(
      Pattern("www.example.com").Matches(GURL("http://www.example.com/")));
  EXPECT_TRUE(
      Pattern("file:///tmp/test.html").Matches(GURL("file:///tmp/test.html")));
  EXPECT_FALSE(Pattern(std::string()).Matches(GURL("http://www.example.com/")));
  EXPECT_FALSE(Pattern("[*.]example.com").Matches(GURL("http://example.org/")));
  EXPECT_FALSE(Pattern("example.com").Matches(GURL("http://example.org/")));
  EXPECT_FALSE(
      Pattern("file:///tmp/test.html").Matches(GURL("file:///tmp/other.html")));
  EXPECT_FALSE(
      Pattern("file:///tmp/test.html").Matches(GURL("http://example.org/")));
}

TEST(ContentSettingsPatternTest, CanonicalizePattern_Legacy) {
  // Basic patterns.
  EXPECT_STREQ("[*.]ikea.com", Pattern("[*.]ikea.com").ToString().c_str());
  EXPECT_STREQ("example.com", Pattern("example.com").ToString().c_str());
  EXPECT_STREQ("192.168.1.1", Pattern("192.168.1.1").ToString().c_str());
  EXPECT_STREQ("[::1]", Pattern("[::1]").ToString().c_str());
  EXPECT_STREQ("file:///tmp/file.html",
               Pattern("file:///tmp/file.html").ToString().c_str());

  // UTF-8 patterns.
  EXPECT_STREQ("[*.]xn--ira-ppa.com",
               Pattern("[*.]\xC4\x87ira.com").ToString().c_str());
  EXPECT_STREQ("xn--ira-ppa.com",
               Pattern("\xC4\x87ira.com").ToString().c_str());
  EXPECT_STREQ("file:///%C4%87ira.html",
               Pattern("file:///\xC4\x87ira.html").ToString().c_str());

  // file:/// normalization.
  EXPECT_STREQ("file:///tmp/test.html",
               Pattern("file:///tmp/bar/../test.html").ToString().c_str());

  // Invalid patterns.
  EXPECT_STREQ("", Pattern("*example.com").ToString().c_str());
  EXPECT_STREQ("", Pattern("example.*").ToString().c_str());
  EXPECT_STREQ("", Pattern("*\xC4\x87ira.com").ToString().c_str());
  EXPECT_STREQ("", Pattern("\xC4\x87ira.*").ToString().c_str());
}

TEST(ContentSettingsPatternTest, Schemes) {
  EXPECT_EQ(ContentSettingsPattern::SCHEME_HTTP,
            Pattern("http://www.example.com").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_HTTPS,
            Pattern("https://www.example.com").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_FILE,
            Pattern("file:///tmp/file.html").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_CHROMEEXTENSION,
            Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
                .GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_CHROMESEARCH,
            Pattern("chrome-search://local-ntp/").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_WILDCARD,
            Pattern("192.168.0.1").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_WILDCARD,
            Pattern("www.example.com").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_OTHER,
            Pattern("filesystem:http://www.google.com/temporary/").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_CHROME,
            Pattern("chrome://sample/").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_CHROMEUNTRUSTED,
            Pattern("chrome-untrusted://sample/").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_DEVTOOLS,
            Pattern("devtools://devtools/").GetScheme());
  EXPECT_EQ(ContentSettingsPattern::SCHEME_ISOLATEDAPP,
            Pattern("isolated-app://"
                    "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/")
                .GetScheme());
}

TEST(ContentSettingsPatternTest, MatchesSingleOrigin) {
  EXPECT_FALSE(Pattern("*").MatchesSingleOrigin());
  EXPECT_FALSE(Pattern("*://example.com:443").MatchesSingleOrigin());
  EXPECT_FALSE(Pattern("https://[*.]example.com:443").MatchesSingleOrigin());
  EXPECT_FALSE(Pattern("https://example.com:*").MatchesSingleOrigin());
  EXPECT_FALSE(Pattern("*://[*.]example.com:*").MatchesSingleOrigin());
  EXPECT_FALSE(Pattern("https://*").MatchesSingleOrigin());
  EXPECT_FALSE(Pattern("file:///*").MatchesSingleOrigin());

  EXPECT_TRUE(Pattern("https://example.com:443").MatchesSingleOrigin());
  EXPECT_TRUE(Pattern("file:///foo/bar/example.txt").MatchesSingleOrigin());

  // URL conversion.
  EXPECT_FALSE(ContentSettingsPattern::FromURL(GURL("https://example.com"))
                   .MatchesSingleOrigin());
  EXPECT_TRUE(
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://example.com"))
          .MatchesSingleOrigin());
  EXPECT_TRUE(
      ContentSettingsPattern::FromURL(GURL("file:///foo/bar/example.txt"))
          .MatchesSingleOrigin());
}

TEST(ContentSettingsPatternTest, ToRepresentativeUrl) {
  EXPECT_EQ(Pattern("*").ToRepresentativeUrl(), GURL());
  EXPECT_EQ(Pattern("https://*").ToRepresentativeUrl(), GURL());

  EXPECT_EQ(Pattern("https://example.com:443").ToRepresentativeUrl(),
            GURL("https://example.com"));
  EXPECT_EQ(Pattern("https://foo.com:*").ToRepresentativeUrl(),
            GURL("https://foo.com"));
  EXPECT_EQ(Pattern("*://example.com:443").ToRepresentativeUrl(),
            GURL("https://example.com"));
  EXPECT_EQ(Pattern("*://example.com:4443").ToRepresentativeUrl(),
            GURL("https://example.com:4443"));
  EXPECT_EQ(Pattern("https://[*.]example.com:443").ToRepresentativeUrl(),
            GURL("https://example.com"));
  EXPECT_EQ(Pattern("*://[*.]example.com:*").ToRepresentativeUrl(),
            GURL("https://example.com"));

  EXPECT_EQ(Pattern("http://example.com").ToRepresentativeUrl(),
            GURL("http://example.com"));
  EXPECT_EQ(Pattern("http://example.com:8080").ToRepresentativeUrl(),
            GURL("http://example.com:8080"));

  EXPECT_EQ(Pattern("chrome://settings").ToRepresentativeUrl(),
            GURL("chrome://settings"));

  EXPECT_EQ(Pattern("file:///*").ToRepresentativeUrl(), GURL());
  EXPECT_EQ(Pattern("file:///foo/bar/example.txt").ToRepresentativeUrl(),
            GURL("file:///foo/bar/example.txt"));
}

TEST(ContentSettingsPatternTest, CompareDomains) {
  ContentSettingsPattern::CompareDomains less;
  EXPECT_TRUE(less("a", "b"));
  EXPECT_FALSE(less("b", "a"));
  EXPECT_TRUE(less("a.b", "b"));
  EXPECT_FALSE(less("b", "a.b"));
  EXPECT_TRUE(less("c.b", "b"));
  EXPECT_FALSE(less("b", "c.b"));
  EXPECT_FALSE(less("c.b", "a.b"));

  std::vector<std::string> domains{
      "b",
      "a",
      "c.b",
      "a.b",
  };
  std::sort(domains.begin(), domains.end(), less);
  std::vector<std::string> expected{
      "a",
      "a.b",
      "c.b",
      "b",
  };
  EXPECT_THAT(domains, testing::ContainerEq(expected));
}
