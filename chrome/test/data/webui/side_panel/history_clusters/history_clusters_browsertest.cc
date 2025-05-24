// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class SidePanelHistoryClustersTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelHistoryClustersTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            history_clusters::internal::kJourneys,
            history_embeddings::kHistoryEmbeddings,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementHistoryEmbedding,
#endif
        },
        {});
    set_test_loader_host(chrome::kChromeUIHistoryClustersSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelHistoryClustersTest, App) {
  RunTest("side_panel/history_clusters/history_clusters_app_test.js",
          "mocha.run()");
}
