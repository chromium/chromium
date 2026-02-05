// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::VariantWith;

std::unique_ptr<TestingProfile> CreateTestingProfile() {
  TestingProfile::Builder builder;
  return builder.Build();
}

class WebContentsContainer {
 public:
  explicit WebContentsContainer(content::BrowserContext& context)
      : context_(context) {}

  content::WebContents& web_contents() { return *web_contents_; }

 private:
  raw_ref<content::BrowserContext> context_;

  std::unique_ptr<content::WebContents> web_contents_ =
      content::WebContents::Create(
          content::WebContents::CreateParams(&*context_));
};

class NonInstalledBundleInspectionContextTest : public ::testing::Test {
 protected:
  content::WebContents& web_contents() {
    return web_contents_container_.web_contents();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;

  std::unique_ptr<TestingProfile> profile_ = CreateTestingProfile();
  WebContentsContainer web_contents_container_{/*context=*/*profile_};
};

TEST_F(NonInstalledBundleInspectionContextTest,
       CanSetAndGetIsolatedWebAppLocation) {
  NonInstalledBundleInspectionContext::CreateForWebContents(
      &web_contents(),
      IwaSourceProxy{url::Origin::Create(GURL("https://example.com"))},
      IwaInstallOperation{
          .source = webapps::WebappInstallSource::IWA_EXTERNAL_POLICY});
  auto* install_info =
      NonInstalledBundleInspectionContext::FromWebContents(&web_contents());

  EXPECT_THAT(
      install_info->source(),
      Eq(IwaSourceProxy{url::Origin::Create(GURL("https://example.com"))}));
}

TEST_F(NonInstalledBundleInspectionContextTest,
       CanSetAndGetAnotherIsolatedWebAppLocation) {
  NonInstalledBundleInspectionContext::CreateForWebContents(
      &web_contents(),
      IwaSourceBundleProdMode{
          base::FilePath{FILE_PATH_LITERAL("some testing bundle path")}},
      IwaMetadataReadingOperation{});
  auto* install_info =
      NonInstalledBundleInspectionContext::FromWebContents(&web_contents());

  EXPECT_THAT(install_info->source(),
              Eq(IwaSourceBundleProdMode{base::FilePath{
                  FILE_PATH_LITERAL("some testing bundle path")}}));
}

}  // namespace
}  // namespace web_app
