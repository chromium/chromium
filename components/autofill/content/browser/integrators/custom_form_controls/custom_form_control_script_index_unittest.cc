// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_index.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::Optional;

// Tests that FindScript() returns the script of the first entry whose host and
// `url_filter` match the URL.
TEST(CustomFormControlScriptIndexTest, FindScriptMatchesHostAndPathForRequest) {
  CustomFormControlScriptIndex index;
  EXPECT_EQ(index.FindScript(GURL("https://example.com/")), std::nullopt);
  index.SetEntries({
      {.host = "shop.example.com",
       .url_filter = "",
       .script = "subdomain_script"},
      {.host = "Example.COM",
       .url_filter = "/checkout",
       .script = "checkout_script"},
      {.host = "example.com",
       .url_filter = R"(index\.php\?cl=user)",
       .script = "query_script"},
      {.host = "example.com", .url_filter = "", .script = "fallback_script"},
  });

  // Subdomain entry matches shop.example.com and its subdomains, not parent.
  EXPECT_THAT(index.FindScript(GURL("https://shop.example.com/cart")),
              Optional(std::string("subdomain_script")));
  EXPECT_THAT(index.FindScript(GURL("https://us.shop.example.com/cart")),
              Optional(std::string("subdomain_script")));

  // Matches normalized host, subdomains, custom ports, and path/query filter.
  EXPECT_THAT(index.FindScript(GURL("https://www.example.com:8443/checkout")),
              Optional(std::string("checkout_script")));
  EXPECT_THAT(index.FindScript(GURL("http://a.b.example.com/shop/checkout")),
              Optional(std::string("checkout_script")));
  EXPECT_THAT(
      index.FindScript(GURL("https://example.com/index.php?cl=user&step=1")),
      Optional(std::string("query_script")));
  EXPECT_THAT(index.FindScript(GURL("https://example.com/signup")),
              Optional(std::string("fallback_script")));

  // URL fragment (#...) is excluded from `url_filter` matching.
  EXPECT_THAT(index.FindScript(GURL("https://example.com/login#/checkout")),
              Optional(std::string("fallback_script")));

  // Non-matching hosts and non-HTTP(S) schemes return nullopt.
  EXPECT_EQ(index.FindScript(GURL("https://notexample.com/checkout")),
            std::nullopt);
  EXPECT_EQ(index.FindScript(GURL("file:///checkout")), std::nullopt);
  EXPECT_EQ(index.FindScript(GURL("about:blank")), std::nullopt);
  EXPECT_EQ(index.FindScript(GURL()), std::nullopt);
}

// Tests that SetEntries() drops entries with an invalid host, `url_filter` or
// script, and keeps the valid ones.
TEST(CustomFormControlScriptIndexTest, SetEntriesDropsInvalidEntries) {
  CustomFormControlScriptIndex index;
  index.SetEntries({
      // Invalid hosts:
      {.host = "", .url_filter = "", .script = "empty_host"},
      {.host = "com", .url_filter = "", .script = "public_suffix"},
      {.host = "co.uk", .url_filter = "", .script = "public_suffix_2"},
      {.host = "github.io", .url_filter = "", .script = "private_registry"},
      {.host = ".example.com", .url_filter = "", .script = "leading_dot"},
      {.host = "example.com.", .url_filter = "", .script = "trailing_dot"},
      {.host = "example.com:443", .url_filter = "", .script = "with_port"},
      {.host = "example.com/path", .url_filter = "", .script = "with_path"},
      // Invalid `url_filter`:
      {.host = "example.com", .url_filter = "[", .script = "bad_regex"},
      // Invalid scripts:
      {.host = "example.com", .url_filter = "", .script = ""},
      {.host = "example.com",
       .url_filter = "",
       .script = std::string(kMaxCustomFormControlScriptSizeBytes + 1, 'x')},
      // Valid entries:
      {.host = "foo.github.io", .url_filter = "", .script = "private_ok"},
      {.host = "example.com", .url_filter = "", .script = "valid_script"},
  });
  EXPECT_THAT(index.FindScript(GURL("https://example.com/")),
              Optional(std::string("valid_script")));
  EXPECT_THAT(index.FindScript(GURL("https://foo.github.io/")),
              Optional(std::string("private_ok")));
  EXPECT_EQ(index.FindScript(GURL("https://bar.github.io/")), std::nullopt);
  EXPECT_EQ(index.FindScript(GURL("https://other.com/")), std::nullopt);
}

// Tests that SetEntries() replaces all previously set entries.
TEST(CustomFormControlScriptIndexTest, SetEntriesReplacesPreviousEntries) {
  CustomFormControlScriptIndex index;
  index.SetEntries({
      {.host = "example.com", .url_filter = "", .script = "old_script"},
      {.host = "other.com", .url_filter = "", .script = "other_script"},
  });
  index.SetEntries(
      {{.host = "example.com", .url_filter = "", .script = "new_script"}});
  EXPECT_THAT(index.FindScript(GURL("https://example.com/")),
              Optional(std::string("new_script")));
  EXPECT_EQ(index.FindScript(GURL("https://other.com/")), std::nullopt);
}

}  // namespace
}  // namespace autofill
