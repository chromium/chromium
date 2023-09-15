// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"

#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::WebAccessibleResourcesInfo;

class WebAccessibleResourcesManifestTest : public ChromeManifestTest {
 protected:
  ManifestData GetManifestData(const std::string& web_accessible_resources,
                               int manifest_version = 3) {
    constexpr char kManifestStub[] =
        R"({
            "name": "Test",
            "version": "0.1",
            "manifest_version": %d,
            "web_accessible_resources": %s
        })";
    return ManifestData::FromJSON(base::StringPrintf(
        kManifestStub, manifest_version, web_accessible_resources.c_str()));
  }
};

TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResources) {
  const auto example_origin =
      url::Origin::Create(GURL("https://example.com/test"));

  // No web_accessible_resources.
  scoped_refptr<Extension> none(
      LoadAndExpectSuccess("web_accessible_resources/v2/none.json"));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(none.get()));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      none.get(), "test", &example_origin));

  // web_accessible_resources: ["test"].
  scoped_refptr<Extension> single(
      LoadAndExpectSuccess("web_accessible_resources/v2/single.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(single.get()));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      single.get(), "test", &example_origin));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      single.get(), "other", &example_origin));

  // web_accessible_resources: ["*"].
  scoped_refptr<Extension> wildcard(
      LoadAndExpectSuccess("web_accessible_resources/v2/wildcard.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(wildcard.get()));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard.get(), "anything", &example_origin));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard.get(), "path/anything", &example_origin));

  // web_accessible_resources: ["path/*.ext"].
  scoped_refptr<Extension> pattern(
      LoadAndExpectSuccess("web_accessible_resources/v2/pattern.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(pattern.get()));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      pattern.get(), "path/anything.ext", &example_origin));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      pattern.get(), "anything.ext", &example_origin));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      pattern.get(), "path/anything.badext", &example_origin));
}

// Tests valid configurations of the web_accessible_resources key in manifest
// v3 extensions.
TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResourcesV3Valid) {
  struct {
    const char* title;
    const char* web_accessible_resources;
  } test_cases[] = {
      {"Succeed when all keys are defined.",
       R"([
            {
              "resources": ["test"],
              "matches": ["https://allowed.example/*"],
              "extension_ids": ["abcdefghijlkmnopabcdefghijklmnop"],
              "use_dynamic_url": true
            }
          ])"},
      {"Succeed if only specifying |extension_ids| and |resources|.",
       R"([
            {
              "resources": ["allowed"],
              "extension_ids": ["abcdefghijlkmnopabcdefghijklmnop"]
            }
          ])"},
      {"Succeed if only specifying |matches| and |resources|.",
       R"([
            {
              "resources": ["test"],
              "matches": ["https://allowed.example/*"]
            }
          ])"},
      {"Succeed if there are multiple objects.",
       R"([
            {
              "resources": ["test"],
              "matches": ["https://allowed.example/*"],
              "extension_ids": ["abcdefghijlkmnopabcdefghijklmnop"]
            },
            {
              "resources": ["two"],
              "matches": ["https://example.org/*"],
              "extension_ids": ["abcdefghijlkmnopabcdefghijklmnop"]
            }
          ])"},
      {"Succeed if unexpected key exists in entry.",
       R"([
            {
              "resources": ["test"],
              "matches": ["https://chromium.org/*"],
              "extension_ids": ["abcdefghijklmnopabcdefghijklmnop"],
              "unexpected_key": ["allowed"]
            }
          ])"},
      {"Succeed if use_dynamic_url key is true, irrespective of matches or "
       "extension_ids.",
       R"([
            {
              "resources": ["test"],
              "use_dynamic_url": true
            }
          ])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        GetManifestData(test_case.web_accessible_resources)));
    EXPECT_TRUE(
        WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
  }
}

// Tests invalid configurations of the web_accessible_resources key in manifest
// v3 extensions.
TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResourcesV3Invalid) {
  struct {
    const char* title;
    const char* web_accessible_resources;
    const char* expected_error;
  } test_cases[] = {
      {"Error if web_accessible_resources key is of incorrect type.", "{}",
       "Error at key 'web_accessible_resources'. Type is invalid. Expected "
       "list, found dictionary."},
      {"Error if objects has no keys.", "[{}]",
       "Error at key 'web_accessible_resources'. Parsing array failed at index "
       "0: 'resources' is required"},
      {"Error if entry only contains |resources|.",
       R"([
            {
              "resources": ["error"]
            }
        ])",
       "Invalid value for 'web_accessible_resources[0]'. Entry "
       "must at least have resources, and one other valid key."},
      {"Error if use_dynamic_url is false, and missing extension_ids or "
       "matches.",
       R"([
            {
              "resources": ["test"],
              "use_dynamic_url": false
            }
          ])",
       "Invalid value for 'web_accessible_resources[0]'. Entry "
       "must at least have resources, and one other valid key."},
      {"Error if incorrect keyed object type is present.",
       R"([
            {
              "resources": ["test"],
              "matches": {"a": "https://error.example/*"},
              "extension_ids": ["abcdefghijlkmnopabcdefghijklmnop"]
            }
        ])",
       "Error at key 'web_accessible_resources'. Parsing array failed at index "
       "0: 'matches': expected list, got dictionary"},
      {"Error if incorrect keyed object type is present.",
       R"([
            {
              "resources": ["test"],
              "matches": [1, 2, 3],
              "extension_ids": [1, 2, 3]
            }
        ])",
       "Error at key 'web_accessible_resources'. Parsing array failed at index "
       "0: Error at key 'matches': Parsing array failed at index 0: expected "
       "string, got integer"},
      {"Error if extension id is invalid.",
       R"([
            {
              "resources": ["test"],
              "matches": ["https://example.com/*"],
              "extension_ids": [-1]
            }
        ])",
       "Error at key 'web_accessible_resources'. Parsing array failed at index "
       "0: Error at key 'extension_ids': Parsing array failed at index 0: "
       "expected string, got integer"},
      {"Error if extension id is invalid.",
       R"([
            {
              "resources": ["test"],
              "matches": ["https://example.com/*"],
              "extension_ids": ["error"]
            }
        ])",
       "Invalid value for 'web_accessible_resources[0]'. Invalid extension "
       "id."},
      {"Error if any match in matches includes a path after the origin.",
       R"([
            {
              "resources": ["test"],
              "matches": [
                "https://allowed.example/*",
                "https://example.com/error*"
              ]
            }
        ])",
       "Invalid value for 'web_accessible_resources[0]'. Invalid match pattern"
       "."},
      {"Only one wildcard is allowed.",
       R"([
         {
           "resources": ["test"],
           "matches": ["<all_urls>"],
           "extension_ids": ["*", "*"]
         }
       ])",
       "Invalid value for 'web_accessible_resources[0]'. If a wildcard entry "
       "is present, it must be the only entry."},
      {"A wildcard with an extension id is invalid.",
       R"([
         {
           "resources": ["test"],
           "matches": ["<all_urls>"],
           "extension_ids": ["abcdefghijlkmnopabcdefghijklmnop", "*"]
         }
       ])",
       "Invalid value for 'web_accessible_resources[0]'. If a wildcard entry "
       "is present, it must be the only entry."},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectError(GetManifestData(test_case.web_accessible_resources),
                       test_case.expected_error);
  }
}

// Succeed if site requesting resource exists in matches.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesOriginRequestingResourceExistsInMatches) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(GetManifestData(
      R"([
        {
          "resources": ["test"],
          "matches": ["https://allowed.example/*"]
        }
    ])")));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
  const auto allowed_origin =
      url::Origin::Create(GURL("https://allowed.example"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension.get(), "test", &allowed_origin));
  const auto error_origin = url::Origin::Create(GURL("http://error.example"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension.get(), "test", &error_origin));
  const auto empty_origin = url::Origin::Create(GURL());
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension.get(), "test", &empty_origin));
}

// Error if V2 uses is keyed to anything other than array of string.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV2MustBeArrayOfString) {
  constexpr char kWebAccessibleResources[] =
      R"([
          {
            "resources": ["test"],
            "matches": ["https://error.example/*"],
          }
      ])";
  LoadAndExpectError(GetManifestData(kWebAccessibleResources, 2),
                     "Error at key 'web_accessible_resources'. Parsing array "
                     "failed at index 0: expected string, got dictionary");
}

// Error if V2's web_accessible_resources key is composed of invalid type.
TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResourcesV2Type) {
  LoadAndExpectSuccess(GetManifestData(R"([])", 2));
  LoadAndExpectSuccess(GetManifestData(R"([""])", 2));
  LoadAndExpectError(GetManifestData(R"([{}])", 2),
                     "Error at key 'web_accessible_resources'. Parsing array "
                     "failed at index 0: expected string, got dictionary");
}

// Restrict resource access by specifying |extension_ids|.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesTestExtensionIds) {
  auto get_manifest_data = [](std::string extension_id =
                                  "abcdefghijklmnopabcdefghijklmnop") {
    constexpr char kManifestStub[] =
        R"({
              "name": "Test",
              "version": "0.1",
              "manifest_version": 3,
              "web_accessible_resources": [
                {
                  "resources": ["test"],
                  "extension_ids": ["%s"]
                }
              ]
          })";
    return ManifestData::FromJSON(
        base::StringPrintf(kManifestStub, extension_id.c_str()));
  };
  scoped_refptr<const Extension> extension_callee =
      LoadAndExpectSuccess(get_manifest_data());
  scoped_refptr<const Extension> extension_caller =
      LoadAndExpectSuccess(get_manifest_data(extension_callee->id()));
  const auto caller_origin = url::Origin::Create(extension_caller->url());
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension_caller.get(), "test", &caller_origin));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension_caller.get(), "inaccessible", &caller_origin));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension_callee.get(), "test", &caller_origin));

  // Test web accessible resource access by specifying an extension wildcard.
  scoped_refptr<const Extension> wildcard_extension =
      LoadAndExpectSuccess(get_manifest_data("*"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard_extension.get(), "test", &caller_origin));
  const auto web_origin = url::Origin::Create(GURL("http://example.com"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard_extension.get(), "test", &web_origin));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard_extension.get(), "inaccessible", &caller_origin));
}

// Tests wildcards of matches.
TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResourcesWildcard) {
  struct {
    const char* title;
    const char* web_accessible_resources;
  } test_cases[] = {
      // clang-format off
    {"Succeed if text based wildcard is used.",
      R"([
        {
          "resources": ["test"],
          "matches": ["<all_urls>"]
        }
      ])"
    },
    {"Succeed if asterisk based wildcard is used.",
      R"([
        {
          "resources": ["test"],
          "matches": ["*://*/*"]
        }
      ])"
    }
      // clang-format on
  };
  const auto allowed_origin =
      url::Origin::Create(GURL("https://allowed.example"));
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        GetManifestData(test_case.web_accessible_resources)));
    EXPECT_TRUE(
        WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
    EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
        extension.get(), "test", &allowed_origin));
  }
}

// Verify behavior of web accessible resources with subdomains and ports.
TEST_F(WebAccessibleResourcesManifestTest, MatchFromInitiator) {
  auto test_match_from_initiator = [&](const char* match,
                                       const char* initiator_string,
                                       bool expected_accessible) {
    // Install extension.
    auto manifest = content::JsReplace(R"([{
          "resources": ["web_accessible_resource.html"],
          "matches": [$1]
        }])",
                                       match);
    scoped_refptr<Extension> extension(
        LoadAndExpectSuccess(GetManifestData(manifest)));
    EXPECT_TRUE(
        WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));

    // Verify behavior of web accessible resources.
    const auto origin = url::Origin::Create(GURL(initiator_string));
    EXPECT_EQ(expected_accessible,
              WebAccessibleResourcesInfo::IsResourceWebAccessible(
                  extension.get(), "web_accessible_resource.html", &origin));
  };

  struct {
    const char* match;
    const char* origin;
    bool expected_accessible;
  } test_cases[] = {
      // Subdomain
      {"https://a.example.com/*", "https://a.example.com/test", true},
      {"https://a.example.com/*", "https://b.example.com/test", false},
      {"https://a.example.com/*", "https://a.example.com:8080/test", true},
      {"https://a.example.com:8080/*", "https://a.example.com/test", false},

      // Wildcard subdomain
      {"https://*.example.com/*", "https://a.example.com/test", true},
      // Wildcard port
      {"https://a.example.com:*/*", "https://a.example.com:8080", true},
      // Mismatched port when port explicitly specified in both
      {"https://a.example.com:8888/*", "https://a.example.com:8080", false},
  };
  for (const auto& test_case : test_cases) {
    test_match_from_initiator(test_case.match, test_case.origin,
                              test_case.expected_accessible);
  }
}

// Verify whether dynamic url should be used.
TEST_F(WebAccessibleResourcesManifestTest, ShouldUseDynamicUrl) {
  // Common uses of web accessible resources.
  auto test_should_use_dynamic_url = [&](bool use_dynamic_url,
                                         const char* resource,
                                         bool expected_use_dynamic_url) {
    auto manifest = content::JsReplace(
        // clang-format off
      R"([{
        "resources": ["web_accessible_resource.html"],
        "matches": ["<all_urls>"],
        "use_dynamic_url": $1
      }])",
        // clang-format on
        use_dynamic_url);
    scoped_refptr<Extension> extension(
        LoadAndExpectSuccess(GetManifestData(manifest)));
    EXPECT_EQ(expected_use_dynamic_url,
              WebAccessibleResourcesInfo::ShouldUseDynamicUrl(extension.get(),
                                                              resource));
  };
  struct {
    bool use_dynamic_url;
    const char* resource;
    bool expected_use_dynamic_url;
  } test_cases[] = {
      {true, "web_accessible_resource.html", true},
      {false, "web_accessible_resource.html", false},
      {true, "undefined.html", false},
      {false, "undefined.html", false},
  };
  for (const auto& test_case : test_cases) {
    test_should_use_dynamic_url(test_case.use_dynamic_url, test_case.resource,
                                test_case.expected_use_dynamic_url);
  }

  // Web accessible rexources undefined.
  constexpr char kManifestStub[] =
      R"({
        "name": "Test",
        "version": "1.0",
        "manifest_version": 3
    })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifestStub));
  EXPECT_EQ(false, WebAccessibleResourcesInfo::ShouldUseDynamicUrl(
                       extension.get(), "resource.html"));
}
