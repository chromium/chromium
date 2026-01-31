// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/unexportable_keys/features.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

class UnexportableKeysInternalsTest : public WebUIMochaBrowserTest {
 protected:
  UnexportableKeysInternalsTest() {
    set_test_loader_host(chrome::kChromeUIUnexportableKeysInternalsHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      unexportable_keys::kUnexportableKeyDeletion};
};

IN_PROC_BROWSER_TEST_F(UnexportableKeysInternalsTest, All) {
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  RunTest("unexportable_keys_internals/app_test.js", "mocha.run();");
}
