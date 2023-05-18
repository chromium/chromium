// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "components/webapps/browser/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class MlPromotionBrowsertest : public WebAppControllerBrowserTest {
 public:
  MlPromotionBrowsertest() {
    scoped_feature_list_.InitAndEnableFeature(
        webapps::features::kWebAppsMlUkmCollection);
  }
  ~MlPromotionBrowsertest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MlPromotionBrowsertest, DISABLED_MetricsCollection) {
  // TODO(b/279521783): Visit an installable page & verify UKM metrics are
  // recorded.
}

}  // namespace
}  // namespace web_app
