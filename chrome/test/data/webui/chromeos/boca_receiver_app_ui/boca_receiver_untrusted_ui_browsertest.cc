// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_receiver_app_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class BocaReceiverUntrustedUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  BocaReceiverUntrustedUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(ash::kBocaReceiverHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kBocaReceiverApp};
};

IN_PROC_BROWSER_TEST_F(BocaReceiverUntrustedUIBrowserTest, ClientApi) {
  RunTestWithoutTestLoader(
      "chromeos/boca_receiver_app_ui/boca_receiver_app_test.js", "mocha.run()");
}
