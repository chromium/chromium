// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_colors_source_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/color/color_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;

// Matcher for `blink::mojom::LocalResourceSource::headers`.
MATCHER_P(HasHeader, header_matcher, "") {
  return ExplainMatchResult(header_matcher, arg.headers, result_listener);
}

// Matcher for a resource in
// `blink::mojom::LocalResourceSource::path_to_resource_map`.
// Verifies that the map contains `path`, and that the resource value is a
// response body matching `content_matcher`.
MATCHER_P2(HasResource, path, content_matcher, "") {
  auto it = arg.path_to_resource_map.find(path);
  if (it == arg.path_to_resource_map.end()) {
    *result_listener << "does not contain resource '" << path << "'";
    return false;
  }
  const auto& value = it->second;
  if (!value->is_response_body()) {
    *result_listener << "resource '" << path << "' is not a response body";
    return false;
  }
  std::string content(value->get_response_body().begin(),
                      value->get_response_body().end());
  return ExplainMatchResult(content_matcher, content, result_listener);
}

// Matcher for `blink::mojom::LocalResourceLoaderConfig`.
// Verifies that the config contains a source for "chrome://theme/" that matches
// `source_matcher`.
MATCHER_P(ContainsThemeSource, source_matcher, "") {
  url::Origin theme_origin = url::Origin::Create(GURL("chrome://theme/"));
  auto it = arg.sources.find(theme_origin);
  if (it == arg.sources.end()) {
    *result_listener << "does not contain theme source at " << theme_origin;
    return false;
  }
  return ExplainMatchResult(source_matcher, it->second, result_listener);
}

}  // namespace

class ThemeColorsSourceManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  ThemeColorsSourceManagerTest() {
    feature_list_.InitAndEnableFeature(
        features::kWebUIInProcessResourceLoadingV2);
  }
  ~ThemeColorsSourceManagerTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the correct CSS is generated when a valid ColorProvider is
// present, and that the resource is mapped under the theme origin.
TEST_F(ThemeColorsSourceManagerTest,
       PopulateLocalResourceLoaderConfig_GeneratesCss) {
  ThemeColorsSourceManager* manager =
      ThemeColorsSourceManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(manager);

  blink::mojom::LocalResourceLoaderConfig config;
  url::Origin requesting_origin =
      url::Origin::Create(GURL("chrome://webui-toolbar.top-chrome/"));

  ui::ColorProvider color_provider;
  manager->SetColorProviderForTesting(&color_provider);

  manager->PopulateLocalResourceLoaderConfig(&config, requesting_origin,
                                             web_contents());

  EXPECT_THAT(config,
              ContainsThemeSource(Pointee(AllOf(
                  HasResource("colors.css", Not(IsEmpty())),
                  // Implementation adds headers if requesting origin != theme
                  // origin, and chrome://webui-toolbar != chrome://theme.
                  HasHeader("Access-Control-Allow-Origin: "
                            "chrome://webui-toolbar.top-chrome")))));
}

// Verifies cross-origin resource access. Checks that the
// Access-Control-Allow-Origin header is set correctly to allow the requesting
// origin (e.g. chrome://foo-bar) to fetch the resource from chrome://theme.
TEST_F(ThemeColorsSourceManagerTest,
       PopulateLocalResourceLoaderConfig_CrossOrigin) {
  ThemeColorsSourceManager* manager =
      ThemeColorsSourceManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(manager);

  blink::mojom::LocalResourceLoaderConfig config;
  url::Origin requesting_origin =
      url::Origin::Create(GURL("chrome://foo-bar/"));
  ui::ColorProvider color_provider;
  manager->SetColorProviderForTesting(&color_provider);

  manager->PopulateLocalResourceLoaderConfig(&config, requesting_origin,
                                             web_contents());

  EXPECT_THAT(config,
              ContainsThemeSource(Pointee(AllOf(
                  HasHeader("Access-Control-Allow-Origin: chrome://foo-bar"),
                  HasResource("colors.css", Not(IsEmpty()))))));
}
