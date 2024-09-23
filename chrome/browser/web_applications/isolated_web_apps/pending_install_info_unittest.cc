// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::Optional;

std::unique_ptr<TestingProfile> CreateTestingProfile() {
  TestingProfile::Builder builder;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

class PendingInstallInfoTest : public ::testing::Test {
 protected:
  content::WebContents& web_contents() {
    return web_contents_container_.web_contents();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;

  std::unique_ptr<TestingProfile> profile_ = CreateTestingProfile();
  WebContentsContainer web_contents_container_{/*context=*/*profile_};
};

TEST_F(PendingInstallInfoTest, SameInstallInfoForTheSameWebContents) {
  auto& one = IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents());
  auto& two = IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents());
  EXPECT_EQ(&one, &two);
}

TEST_F(PendingInstallInfoTest, DifferentInstancesForDifferentWebContents) {
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile();

  WebContentsContainer web_contents_container_one{/*context=*/*profile};
  WebContentsContainer web_contents_container_two{/*context=*/*profile};

  EXPECT_NE(&IsolatedWebAppPendingInstallInfo::FromWebContents(
                web_contents_container_one.web_contents()),
            &IsolatedWebAppPendingInstallInfo::FromWebContents(
                web_contents_container_two.web_contents()));
}

TEST_F(PendingInstallInfoTest, CanSetAndGetIsolatedWebAppLocation) {
  auto& install_info =
      IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents());
  install_info.set_source(
      IwaSourceProxy{url::Origin::Create(GURL("https://example.com"))});

  EXPECT_THAT(install_info.source(),
              Optional(Eq(IwaSourceProxy{
                  url::Origin::Create(GURL("https://example.com"))})));
}

TEST_F(PendingInstallInfoTest, CanSetAndGetAnotherIsolatedWebAppLocation) {
  auto& install_info =
      IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents());
  install_info.set_source(IwaSourceBundleProdMode{
      base::FilePath{FILE_PATH_LITERAL("some testing bundle path")}});

  EXPECT_THAT(install_info.source(),
              Optional(Eq(IwaSourceBundleProdMode{base::FilePath{
                  FILE_PATH_LITERAL("some testing bundle path")}})));
}

TEST_F(PendingInstallInfoTest, IsolatedWebAppLocationIsEmptyAfterReset) {
  auto& install_info =
      IsolatedWebAppPendingInstallInfo::FromWebContents(web_contents());

  EXPECT_THAT(install_info.source().has_value(), IsFalse());

  install_info.set_source(IwaSourceBundleProdMode{
      base::FilePath{FILE_PATH_LITERAL("some testing bundle path")}});

  install_info.ResetSource();

  EXPECT_THAT(install_info.source().has_value(), IsFalse());
}

}  // namespace
}  // namespace web_app
