// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

using blink::mojom::SubAppsService;
using testing::AllOf;
using testing::Eq;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;
using testing::Not;
using testing::SizeIs;

namespace web_app {

namespace {
constexpr const char kSub1[] = "/sub1/page.html";
constexpr const char kSub2[] = "/sub2/page.html";
}  // namespace

typedef base::AutoReset<
    std::optional<SubAppsInstallDialogController::DialogActionForTesting>>
    DialogOverride;

class SubAppsAdminPolicyTest : public IsolatedWebAppBrowserTestHarness {
 public:
  SubAppsAdminPolicyTest() = default;

  ~SubAppsAdminPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    IsolatedWebAppBrowserTestHarness::SetUpInProcessBrowserTestFixture();
  }

  content::RenderFrameHost* InstallAndOpenParentIwaApp() {
    IsolatedWebAppUrlInfo parent_app = InstallIwaParentApp();
    return OpenApp(parent_app.app_id());
  }

  IsolatedWebAppUrlInfo InstallIwaParentApp() {
    iwa_dev_server_ = CreateAndStartDevServer(
        FILE_PATH_LITERAL("web_apps/subapps_isolated_app"));
    IsolatedWebAppUrlInfo parent_app =
        web_app::InstallDevModeProxyIsolatedWebApp(
            browser()->profile(), iwa_dev_server_->GetOrigin());
    parent_app_id_ = parent_app.app_id();

    EXPECT_THAT(provider().registrar_unsafe().IsInstalled(parent_app_id_),
                IsTrue());
    EXPECT_THAT(provider().registrar_unsafe().IsIsolated(parent_app_id_),
                IsTrue());
    EXPECT_THAT(GetAllSubAppIds(parent_app_id_), IsEmpty());

    return parent_app;
  }

  std::vector<webapps::AppId> GetAllSubAppIds(
      const webapps::AppId& parent_app_id) {
    return provider().registrar_unsafe().GetAllSubAppIds(parent_app_id);
  }

  // sub_app_paths should contain paths, not full URLs.
  std::string AddSubAppsScript(
      const std::vector<std::string>& sub_app_paths) const {
    std::vector<std::string> script_parts;
    const std::string format = R"("$1": {"installURL": "$1"},)";

    script_parts.push_back("navigator.subApps.add({");
    for (const std::string& path : sub_app_paths) {
      script_parts.push_back(
          base::ReplaceStringPlaceholders(format, {path}, nullptr));
    }
    script_parts.push_back("})");

    return base::StrCat(script_parts);
  }

  void SetAllowlistedOrigins(
      const std::vector<std::string>& allowlisted_origins) {
    policy::PolicyMap policy_map;
    base::Value::List allowed_origins;

    for (auto& origin : allowlisted_origins) {
      allowed_origins.Append(base::Value(origin));
    }

    policy_map.Set(
        policy::key::
            kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins,
        policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
        policy::POLICY_SOURCE_PLATFORM, base::Value(std::move(allowed_origins)),
        nullptr);
    policy_provider_.UpdateChromePolicy(policy_map);
  }

  DialogOverride AcceptConfirmationDialog() {
    return SubAppsInstallDialogController::SetAutomaticActionForTesting(
        SubAppsInstallDialogController::DialogActionForTesting::kAccept);
  }

  DialogOverride DeclineConfirmationDialog() {
    return SubAppsInstallDialogController::SetAutomaticActionForTesting(
        SubAppsInstallDialogController::DialogActionForTesting::kCancel);
  }

  bool CheckPolicyValue(content::RenderFrameHost* frame) const {
    auto* contents = content::WebContents::FromRenderFrameHost(frame);
    auto const* prefs =
        Profile::FromBrowserContext(frame->GetBrowserContext())->GetPrefs();

    return policy::IsOriginInAllowlist(
        contents->GetURL(), prefs,
        prefs::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins);
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};

  std::unique_ptr<net::EmbeddedTestServer> iwa_dev_server_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  webapps::AppId parent_app_id_;
};

IN_PROC_BROWSER_TEST_F(SubAppsAdminPolicyTest, SucceedsWithGestureNoPolicy) {
  auto* iwa_frame = InstallAndOpenParentIwaApp();
  auto dialog_override = AcceptConfirmationDialog();

  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsFalse());

  auto ret = EvalJs(iwa_frame, AddSubAppsScript({kSub1, kSub2}));

  EXPECT_THAT(ret.error, IsEmpty());
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), SizeIs(2));
}

IN_PROC_BROWSER_TEST_F(SubAppsAdminPolicyTest, FailsNoGestureNoPolicy) {
  auto* iwa_frame = InstallAndOpenParentIwaApp();
  auto dialog_override = DeclineConfirmationDialog();
  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsFalse());

  auto ret = EvalJs(iwa_frame, AddSubAppsScript({kSub1, kSub2}),
                    content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);

  EXPECT_THAT(ret.error, AllOf(Not(IsEmpty()),
                               HasSubstr("This API can only be called shortly "
                                         "after a user activation.")));
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SubAppsAdminPolicyTest, SucceedsNoGestureWithPolicy) {
  auto parent = InstallIwaParentApp();
  SetAllowlistedOrigins({parent.origin().GetURL().spec()});
  auto dialog_override = DeclineConfirmationDialog();
  auto* iwa_frame = OpenApp(parent.app_id());

  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsTrue());

  auto ret1 = EvalJs(iwa_frame, AddSubAppsScript({kSub1}),
                     content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_THAT(ret1.error, IsEmpty());
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), SizeIs(1));

  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsTrue());

  auto ret2 = EvalJs(iwa_frame, AddSubAppsScript({kSub2}),
                     content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_THAT(ret2.error, IsEmpty());
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), SizeIs(2));
}

IN_PROC_BROWSER_TEST_F(SubAppsAdminPolicyTest, SucceedsWithPolicyAndGesture) {
  auto parent = InstallIwaParentApp();
  SetAllowlistedOrigins({parent.origin().GetURL().spec()});
  auto dialog_override = DeclineConfirmationDialog();
  auto* iwa_frame = OpenApp(parent.app_id());

  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsTrue());

  auto ret = EvalJs(iwa_frame, AddSubAppsScript({kSub1, kSub2}));
  EXPECT_THAT(ret.error, IsEmpty());
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), SizeIs(2));
}

IN_PROC_BROWSER_TEST_F(SubAppsAdminPolicyTest,
                       SucceedsWithMultipleOriginsInPolicyAndNoGesture) {
  auto parent = InstallIwaParentApp();
  SetAllowlistedOrigins({"https://www.google.com",
                         parent.origin().GetURL().spec(),
                         "https:/another.domain.com"});
  auto dialog_override = DeclineConfirmationDialog();
  auto* iwa_frame = OpenApp(parent.app_id());

  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsTrue());

  auto ret = EvalJs(iwa_frame, AddSubAppsScript({kSub1, kSub2}),
                    content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_THAT(ret.error, IsEmpty());
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), SizeIs(2));
}

IN_PROC_BROWSER_TEST_F(SubAppsAdminPolicyTest,
                       FailsWithDifferentOriginAndNoGesture) {
  auto parent = InstallIwaParentApp();
  SetAllowlistedOrigins({"https://www.google.com/"});
  auto dialog_override = DeclineConfirmationDialog();
  auto* iwa_frame = OpenApp(parent.app_id());

  EXPECT_THAT(CheckPolicyValue(iwa_frame), IsFalse());

  auto ret = EvalJs(iwa_frame, AddSubAppsScript({kSub1, kSub2}),
                    content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);

  EXPECT_THAT(ret.error, AllOf(Not(IsEmpty()),
                               HasSubstr("This API can only be called shortly "
                                         "after a user activation.")));
  EXPECT_THAT(GetAllSubAppIds(parent_app_id_), IsEmpty());
}

}  // namespace web_app
