// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

using Step = IsolatedWebAppInstallerModel::Step;

struct TestParam {
  std::string test_suffix;
  Step step;
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

const TestParam kTestParam[] = {
    {.test_suffix = "Disabled", .step = Step::kDisabled},
    {.test_suffix = "GetMetadata", .step = Step::kGetMetadata},
};

class IsolatedWebAppInstallerViewUiPixelTest
    : public TestBrowserDialog,
      public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  IsolatedWebAppInstallerViewUiPixelTest()
      : pixel_test_mixin_(&mixin_host_,
                          GetParam().use_dark_theme,
                          GetParam().use_right_to_left_language) {}

  ~IsolatedWebAppInstallerViewUiPixelTest() override = default;

  // `TestBrowserDialog`:
  void ShowUi(const std::string& name) override {
    IsolatedWebAppInstallerModel model{base::FilePath()};
    model.SetStep(GetParam().step);

    Profile* profile = browser()->profile();
    IsolatedWebAppInstallerViewController controller{
        profile, WebAppProvider::GetForWebApps(profile), &model};
    controller.Show(base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kIsolatedWebApps};
  PixelTestConfigurationMixin pixel_test_mixin_;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppInstallerViewUiPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         IsolatedWebAppInstallerViewUiPixelTest,
                         testing::ValuesIn(kTestParam),
                         &ParamToTestSuffix);

}  // namespace web_app
