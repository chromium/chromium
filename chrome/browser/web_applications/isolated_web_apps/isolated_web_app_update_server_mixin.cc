// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"

#include "base/files/file_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace web_app {

IsolatedWebAppUpdateServerMixin::IsolatedWebAppUpdateServerMixin(
    InProcessBrowserTestMixinHost* mixin_host,
    InProcessBrowserTest* test_base)
    : InProcessBrowserTestMixin(mixin_host), test_base_(test_base) {}

IsolatedWebAppUpdateServerMixin::~IsolatedWebAppUpdateServerMixin() = default;

void IsolatedWebAppUpdateServerMixin::SetUpOnMainThread() {
  SetUpFilesAndServer();
}

void IsolatedWebAppUpdateServerMixin::TearDownOnMainThread() {
  test_base_ = nullptr;
}

url::Origin IsolatedWebAppUpdateServerMixin::GetAppOrigin() const {
  CHECK(url_info_.has_value());
  return url_info_->origin();
}

webapps::AppId IsolatedWebAppUpdateServerMixin::GetAppId() const {
  CHECK(url_info_.has_value());
  return url_info_->app_id();
}

web_package::SignedWebBundleId IsolatedWebAppUpdateServerMixin::GetWebBundleId()
    const {
  CHECK(url_info_.has_value());
  return url_info_->web_bundle_id();
}

GURL IsolatedWebAppUpdateServerMixin::GetUpdateManifestUrl() const {
  return iwa_server_.GetURL(base::StrCat({"/", kUpdateManifestFileName}));
}

void IsolatedWebAppUpdateServerMixin::SetUpFilesAndServer() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // We cannot use `ScopedTempDir` here because the directory must survive
  // restarts for the `PRE_` tests to work. Use a directory within the profile
  // directory instead.
  temp_dir_ = profile()->GetPath().AppendASCII("iwa-temp-for-testing");
  EXPECT_TRUE(base::CreateDirectory(temp_dir_));
  iwa_server_.ServeFilesFromDirectory(temp_dir_);
  EXPECT_TRUE(iwa_server_.Start());

  auto bundle_id =
      web_package::SignedWebBundleId::CreateForPublicKey(key_pair_.public_key);
  url_info_ = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);

  auto builder = IsolatedWebAppBuilder(
      ManifestBuilder().SetName("app-1.0.0").SetVersion("1.0.0"));
  builder.AddHtml("/", R"(
        <head>
          <title>1.0.0</title>
        </head>
        <body>
          <h1>Hello from version 1.0.0</h1>
        </body>)");
  base::FilePath bundle_path = temp_dir_.Append(kBundleFileName);
  bundle_ = builder.BuildBundle(bundle_path, key_pair_);
  bundle_->TrustSigningKey();

  EXPECT_TRUE(base::WriteFile(
      temp_dir_.Append(kUpdateManifestFileName),
      base::ReplaceStringPlaceholders(
          R"(
              {
                "versions": [
                  {"version": "1.0.0", "src": "$1"}
                ]
              }
            )",
          {iwa_server_.GetURL(base::StrCat({"/", kBundleFileName})).spec()},
          /*offsets=*/nullptr)));
}

Profile* IsolatedWebAppUpdateServerMixin::profile() {
  return test_base_->browser()->profile();
}

}  // namespace web_app
