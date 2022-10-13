// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_browsertest_base.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace chromeos {

using ParentAccessUIBrowserTest = ParentAccessChildUserBrowserTestBase;

// Test cases for ParentAccessUI class
IN_PROC_BROWSER_TEST_F(ParentAccessUIBrowserTest, URLParameters) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);
  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  GURL webview_url = GetParentAccessUI()->GetWebContentURLForTesting();
  ASSERT_TRUE(webview_url.has_query());

  // Split the query string into a map of keys to values.
  std::string query_str = webview_url.query();
  url::Component query(0, query_str.length());
  url::Component key;
  url::Component value;
  std::map<std::string, std::string> query_parts;
  while (url::ExtractQueryKeyValue(query_str.c_str(), &query, &key, &value)) {
    query_parts[query_str.substr(key.begin, key.len)] =
        query_str.substr(value.begin, value.len);
  }

  // Validate the query parameters.
  // TODO(b/200853161): Validate caller id from params.
  EXPECT_EQ(query_parts.at("callerid"), "39454505");
  EXPECT_EQ(query_parts.at("cros-origin"), "chrome://parent-access");
  EXPECT_EQ(query_parts.at("platform_version"),
            base::SysInfo::OperatingSystemVersion());
  EXPECT_EQ(
      query_parts.at("hl"),
      google_util::GetGoogleLocale(g_browser_process->GetApplicationLocale()));
}

}  // namespace chromeos
