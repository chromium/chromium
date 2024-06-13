// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/value_iterators.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kTestApp[] = "https://test.test/";

class ManagementUIPWATest : public web_app::WebAppBrowserTestBase {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDesktopPWAsRunOnOsLogin};
};

IN_PROC_BROWSER_TEST_F(ManagementUIPWATest, RunOnOsLoginApplicationsReported) {
  // Set up policy values and install PWAs
  profile()->GetPrefs()->SetList(
      prefs::kWebAppSettings,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(web_app::kManifestId, kTestApp)
              .Set(web_app::kRunOnOsLogin, web_app::kRunWindowed)));

  const webapps::AppId& app_id = InstallPWA(GURL(kTestApp));

  // Check that applications contains given app
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://management")));

  const std::string javascript =
      "window.ManagementBrowserProxyImpl.getInstance()"
      "  .getApplications()"
      "  .then(result => "
      "    JSON.stringify(result));";

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string actual_json =
      content::EvalJs(contents, javascript).ExtractString();

  std::optional<base::Value> actual_value = base::JSONReader::Read(actual_json);

  ASSERT_TRUE(actual_value.has_value());

  const std::string app_name = web_app::WebAppProvider::GetForTest(profile())
                                   ->registrar_unsafe()
                                   .GetAppShortName(app_id);

  base::Value::List expected_value;
  base::Value::Dict app_info;
  app_info.Set("name", app_name);
  GURL icon = apps::AppIconSource::GetIconURL(
      app_id, extension_misc::EXTENSION_ICON_SMALLISH);
  app_info.Set("icon", icon.spec());
  base::Value::List permission_messages;
  permission_messages.Append(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_APPLICATIONS_RUN_ON_OS_LOGIN));
  app_info.Set("permissions", std::move(permission_messages));
  expected_value.Append(std::move(app_info));

  EXPECT_EQ(actual_value.value(), expected_value);

  base::Value::List& values = actual_value->GetList();
  base::Value& actual_app = values[0];

  ASSERT_EQ(*actual_app.GetDict().FindString("name"), app_name);
}
}  // namespace
