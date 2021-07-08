// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_installation_utils.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

using base::UTF8ToUTF16;

namespace web_app {

namespace {

GURL StartUrl() {
  return GURL("https://www.example.com/index.html");
}

}  // namespace

TEST(WebAppInstallationUtils, SetWebAppManifestFields_Summary) {
  WebApplicationInfo web_app_info;
  web_app_info.start_url = GURL("https://www.chromium.org/index.html");
  web_app_info.scope = web_app_info.start_url.GetWithoutFilename();
  web_app_info.title = u"App Name";
  web_app_info.description = u"App Description";
  web_app_info.theme_color = SK_ColorCYAN;
  web_app_info.background_color = SK_ColorMAGENTA;

  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, web_app_info.start_url);
  auto web_app = std::make_unique<WebApp>(app_id);
  SetWebAppManifestFields(web_app_info, *web_app);

  EXPECT_EQ(web_app->scope(), GURL("https://www.chromium.org/"));
  EXPECT_EQ(web_app->name(), "App Name");
  EXPECT_EQ(web_app->description(), "App Description");
  EXPECT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(*web_app->theme_color(), SK_ColorCYAN);
  EXPECT_TRUE(web_app->background_color().has_value());
  EXPECT_EQ(*web_app->background_color(), SK_ColorMAGENTA);

  web_app_info.theme_color = absl::nullopt;
  web_app_info.background_color = absl::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->theme_color().has_value());
  EXPECT_FALSE(web_app->background_color().has_value());
}

TEST(WebAppInstallationUtils, SetWebAppManifestFields_ShareTarget) {
  WebApplicationInfo web_app_info;
  web_app_info.start_url = StartUrl();
  web_app_info.scope = web_app_info.start_url.GetWithoutFilename();
  web_app_info.title = u"App Name";

  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, web_app_info.start_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  {
    apps::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share1");
    share_target.method = apps::ShareTarget::Method::kPost;
    share_target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
    share_target.params.title = "kTitle";
    share_target.params.text = "kText";

    apps::ShareTarget::Files file_filter;
    file_filter.name = "kImages";
    file_filter.accept.push_back(".png");
    file_filter.accept.push_back("image/png");
    share_target.params.files.push_back(std::move(file_filter));
    web_app_info.share_target = std::move(share_target);
  }

  SetWebAppManifestFields(web_app_info, *web_app);

  {
    EXPECT_TRUE(web_app->share_target().has_value());
    auto share_target = *web_app->share_target();
    EXPECT_EQ(share_target.action, GURL("http://example.com/share1"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kPost);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kMultipartFormData);
    EXPECT_EQ(share_target.params.title, "kTitle");
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_TRUE(share_target.params.url.empty());
    EXPECT_EQ(share_target.params.files.size(), 1U);
    EXPECT_EQ(share_target.params.files[0].name, "kImages");
    EXPECT_EQ(share_target.params.files[0].accept.size(), 2U);
    EXPECT_EQ(share_target.params.files[0].accept[0], ".png");
    EXPECT_EQ(share_target.params.files[0].accept[1], "image/png");
  }

  {
    apps::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share2");
    share_target.method = apps::ShareTarget::Method::kGet;
    share_target.enctype = apps::ShareTarget::Enctype::kFormUrlEncoded;
    share_target.params.text = "kText";
    share_target.params.url = "kUrl";
    web_app_info.share_target = std::move(share_target);
  }

  SetWebAppManifestFields(web_app_info, *web_app);

  {
    EXPECT_TRUE(web_app->share_target().has_value());
    auto share_target = *web_app->share_target();
    EXPECT_EQ(share_target.action, GURL("http://example.com/share2"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kGet);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kFormUrlEncoded);
    EXPECT_TRUE(share_target.params.title.empty());
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_EQ(share_target.params.url, "kUrl");
    EXPECT_TRUE(share_target.params.files.empty());
  }

  web_app_info.share_target = absl::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->share_target().has_value());
}

TEST(WebAppInstallationUtils, SetWebAppManifestFields_LimitFileHandlers) {
  auto action_url = [](unsigned index) {
    return StartUrl().Resolve(base::StringPrintf("a%u", index));
  };

  auto mime_type = [](unsigned index) {
    return base::StringPrintf("application/x-%u", index);
  };

  auto extension = [](unsigned index) {
    return base::StringPrintf(".e%u", index);
  };

  WebApplicationInfo web_app_info;
  web_app_info.start_url = StartUrl();
  web_app_info.scope = web_app_info.start_url.GetWithoutFilename();
  web_app_info.title = u"App Name";

  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, web_app_info.start_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  {
    // Add more than |kMaxFileHandlers| file handlers.
    for (unsigned i = 0; i <= 2 * kMaxFileHandlers; ++i) {
      const std::u16string name = UTF8ToUTF16(base::StringPrintf("n%u", i));
      std::map<std::u16string, std::vector<std::u16string>> accept;
      accept[UTF8ToUTF16(mime_type(i))] = {UTF8ToUTF16(extension(i))};
      web_app_info.file_handlers.push_back(
          {action_url(i), name, std::vector<blink::Manifest::ImageResource>(),
           std::move(accept)});
    }
    EXPECT_GT(web_app_info.file_handlers.size(), kMaxFileHandlers);
  }

  SetWebAppManifestFields(web_app_info, *web_app);

  {
    EXPECT_EQ(web_app->file_handlers().size(), kMaxFileHandlers);
    for (unsigned i = 0; i < kMaxFileHandlers; ++i) {
      EXPECT_EQ(web_app->file_handlers()[i].action, action_url(i));
      EXPECT_EQ(web_app->file_handlers()[i].accept.size(), 1U);
      EXPECT_EQ(web_app->file_handlers()[i].accept[0].mime_type, mime_type(i));
      EXPECT_EQ(web_app->file_handlers()[i].accept[0].file_extensions.size(),
                1U);
      EXPECT_EQ(*web_app->file_handlers()[i].accept[0].file_extensions.begin(),
                extension(i));
    }
  }
}

}  // namespace web_app
