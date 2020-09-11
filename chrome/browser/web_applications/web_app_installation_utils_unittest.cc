// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_installation_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {

TEST(WebAppInstallationUtils, SetWebAppManifestFields_ShareTarget) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL("https://www.chromium.org/index.html");
  web_app_info.scope = web_app_info.app_url.GetWithoutFilename();
  web_app_info.title = base::ASCIIToUTF16("App Name");

  const AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  {
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share1");
    share_target.method = blink::Manifest::ShareTarget::Method::kPost;
    share_target.enctype =
        blink::Manifest::ShareTarget::Enctype::kMultipartFormData;
    share_target.params.title = base::ASCIIToUTF16("kTitle");
    share_target.params.text = base::ASCIIToUTF16("kText");

    blink::Manifest::FileFilter file_filter;
    file_filter.name = base::ASCIIToUTF16("kImages");
    file_filter.accept.push_back(base::ASCIIToUTF16(".png"));
    file_filter.accept.push_back(base::ASCIIToUTF16("image/png"));
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
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share2");
    share_target.method = blink::Manifest::ShareTarget::Method::kGet;
    share_target.enctype =
        blink::Manifest::ShareTarget::Enctype::kFormUrlEncoded;
    share_target.params.text = base::ASCIIToUTF16("kText");
    share_target.params.url = base::ASCIIToUTF16("kUrl");
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

  web_app_info.share_target = base::nullopt;
  SetWebAppManifestFields(web_app_info, *web_app);
  EXPECT_FALSE(web_app->share_target().has_value());
}

}  // namespace web_app
