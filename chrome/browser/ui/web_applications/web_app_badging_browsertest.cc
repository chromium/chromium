// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using content::RenderFrameHost;

namespace web_app {

class WebAppBadgingBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppBadgingBrowserTest()
      : cross_origin_https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();

    ASSERT_TRUE(cross_origin_https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL cross_site_frame_url =
        cross_origin_https_server_.GetURL("/web_app_badging/blank.html");
    cross_site_app_id_ = InstallPWA(cross_site_frame_url);

    // Note: The url for the cross site frame is embedded in the query string.
    GURL start_url = https_server()->GetURL(
        "/web_app_badging/badging_with_frames_and_workers.html?url=" +
        cross_site_frame_url.spec());
    main_app_id_ = InstallPWA(start_url);

    GURL sub_start_url = https_server()->GetURL("/web_app_badging/blank.html");
    auto sub_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(sub_start_url);
    sub_app_info->scope = sub_start_url;
    sub_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    sub_app_id_ = InstallWebApp(std::move(sub_app_info));

    apps::AppReadinessWaiter(profile(), cross_site_app_id_).Await();
    apps::AppReadinessWaiter(profile(), main_app_id_).Await();
    apps::AppReadinessWaiter(profile(), sub_app_id_).Await();

    content::WebContents* web_contents = OpenApplication(main_app_id_);
    // There should be exactly 4 frames:
    // 1) The main frame.
    // 2) A frame containing a sub app.
    // 3) A cross site frame, on |cross_site_frame_url|.
    // 4) A sub frame in the app's scope.
    auto frames = CollectAllRenderFrameHosts(web_contents->GetPrimaryPage());
    ASSERT_EQ(4u, frames.size());

    main_frame_ = web_contents->GetPrimaryMainFrame();
    for (auto* frame : frames) {
      if (frame->GetLastCommittedURL() == sub_start_url) {
        sub_app_frame_ = frame;
      } else if (url::IsSameOriginWith(frame->GetLastCommittedURL(),
                                       main_frame_->GetLastCommittedURL())) {
        in_scope_frame_ = frame;
      } else if (frame != main_frame_) {
        cross_site_frame_ = frame;
      }
    }

    ASSERT_TRUE(main_frame_);
    ASSERT_TRUE(sub_app_frame_);
    ASSERT_TRUE(in_scope_frame_);
    ASSERT_TRUE(cross_site_frame_);

    // Register two service workers:
    // 1) A service worker with a scope that applies to both the main app and
    // the sub app.
    // 2) A service worker with a scope that applies to the sub app only.
    app_service_worker_scope_ = start_url.GetWithoutFilename();
    const std::string register_app_service_worker_script = content::JsReplace(
        kRegisterServiceWorkerScript, app_service_worker_scope_.spec());
    ASSERT_EQ("OK",
              EvalJs(main_frame_.get(), register_app_service_worker_script));

    sub_app_service_worker_scope_ = sub_start_url;
    const std::string register_sub_app_service_worker_script =
        content::JsReplace(kRegisterServiceWorkerScript,
                           sub_app_service_worker_scope_.spec());
    ASSERT_EQ("OK", EvalJs(main_frame_.get(),
                           register_sub_app_service_worker_script));

    awaiter_ = std::make_unique<base::RunLoop>();

    badging::BadgeManager* badge_manager =
        badging::BadgeManagerFactory::GetForProfile(profile());

    // The delegate is owned by the badge manager. We hold a pointer to it for
    // the test.
    std::unique_ptr<badging::TestBadgeManagerDelegate> owned_delegate =
        std::make_unique<badging::TestBadgeManagerDelegate>(profile(),
                                                            badge_manager);
    owned_delegate->SetOnBadgeChanged(base::BindRepeating(
        &WebAppBadgingBrowserTest::OnBadgeChanged, base::Unretained(this)));
    delegate_ = owned_delegate.get();

    badge_manager->SetDelegate(std::move(owned_delegate));
  }

  // WebAppBrowserTestBase:
  void TearDownOnMainThread() override {
    WebAppRegistrar& registrar = provider().registrar_unsafe();
    for (const auto& app_id : registrar.GetAppIds()) {
      web_app::test::UninstallWebApp(profile(), app_id);
      apps::AppReadinessWaiter(profile(), app_id,
                               apps::Readiness::kUninstalledByUser)
          .Await();
    }

    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  void OnBadgeChanged() {
    // This is only set up to deal with one badge change at a time per app,
    // in order to make asserting the result of a badge change easier.  A single
    // service worker badge call may affect multiple apps within its scope.
    const size_t total_changes =
        delegate_->cleared_badges().size() + delegate_->set_badges().size();
    ASSERT_LE(total_changes, expected_badge_change_count_);

    if (expected_badge_change_count_ == total_changes) {
      // Update |badge_change_map_| to record each badge clear and badge set
      // that occurred.
      for (const auto& cleared_app_id : delegate_->cleared_badges()) {
        BadgeChange clear_badge_change;
        clear_badge_change.was_cleared_ = true;

        ASSERT_TRUE(badge_change_map_.find(cleared_app_id) ==
                    badge_change_map_.end())
            << "ERROR: Cannot record badge clear.  App with ID: '"
            << cleared_app_id << "' has multiple badge changes.";

        badge_change_map_[cleared_app_id] = clear_badge_change;
      }
      for (const auto& set_app_badge : delegate_->set_badges()) {
        BadgeChange set_badge_change;
        set_badge_change.last_badge_content_ = set_app_badge.second;
        set_badge_change.was_flagged_ =
            set_badge_change.last_badge_content_ == std::nullopt;

        const webapps::AppId& set_app_id = set_app_badge.first;
        ASSERT_TRUE(badge_change_map_.find(set_app_id) ==
                    badge_change_map_.end())
            << "ERROR: Cannot record badge set.  App with ID: '" << set_app_id
            << "' has multiple badge changes.";

        badge_change_map_[set_app_id] = set_badge_change;
      }

      awaiter_->Quit();
    }
  }

 protected:
  // Expects a single badge change only.
  void ExecuteScriptAndWaitForBadgeChange(const std::string& script,
                                          RenderFrameHost* on) {
    ExecuteScriptAndWaitForMultipleBadgeChanges(
        script, on, /*expected_badge_change_count=*/1);
  }

  // Handles badge changes that may affect multiple apps. Useful for testing
  // service workers, which can control many apps.
  void ExecuteScriptAndWaitForMultipleBadgeChanges(
      const std::string& script,
      RenderFrameHost* on,
      size_t expected_badge_change_count) {
    expected_badge_change_count_ = expected_badge_change_count;
    badge_change_map_.clear();

    awaiter_ = std::make_unique<base::RunLoop>();
    delegate_->ResetBadges();

    ASSERT_TRUE(content::ExecJs(on, script));

    if (badge_change_map_.size() >= expected_badge_change_count_)
      return;

    awaiter_->Run();
  }

  // Runs script in |main_frame_| that posts a message to the service worker
  // specified by |service_worker_scope|.  The service worker's message handler
  // then calls setAppBadge() with |badge_value|.
  void SetBadgeInServiceWorkerAndWaitForChanges(
      const GURL& service_worker_scope,
      std::optional<uint64_t> badge_value,
      size_t expected_badge_change_count) {
    std::string message_data;
    if (badge_value) {
      message_data = "{ command: 'set-app-badge', value: " +
                     base::NumberToString(*badge_value) + "}";
    } else {
      message_data = "{ command: 'set-app-badge' }";
    }

    ExecuteScriptAndWaitForMultipleBadgeChanges(
        "postMessageToServiceWorker('" + service_worker_scope.spec() + "', " +
            message_data + ")",
        main_frame_, expected_badge_change_count);
  }

  // Same as SetBadgeInServiceWorkerAndWaitForChanges() above, except runs
  // clearAppBadge() in the service worker.
  void ClearBadgeInServiceWorkerAndWaitForChanges(
      const GURL& service_worker_scope,
      size_t expected_badge_change_count) {
    ExecuteScriptAndWaitForMultipleBadgeChanges(
        "postMessageToServiceWorker('" + service_worker_scope.spec() +
            "', { command: 'clear-app-badge' });",
        main_frame_, expected_badge_change_count);
  }

  const webapps::AppId& main_app_id() { return main_app_id_; }
  const webapps::AppId& sub_app_id() { return sub_app_id_; }
  const webapps::AppId& cross_site_app_id() { return cross_site_app_id_; }

  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> main_frame_ = nullptr;
  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> sub_app_frame_ =
      nullptr;
  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> in_scope_frame_ =
      nullptr;
  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> cross_site_frame_ =
      nullptr;

  // Use this script text with EvalJs() on |main_frame_| to register a service
  // worker.  Use ReplaceJs() to replace $1 with the service worker scope URL.
  const std::string kRegisterServiceWorkerScript =
      "registerServiceWorker('service_worker.js', $1);";

  // Both the main app and sub app are within this scope.
  GURL app_service_worker_scope_;

  // Only the sub app is within this scope.
  GURL sub_app_service_worker_scope_;

  // Frame badge updates affect the badge for at most 1 app.  However, a single
  // service worker badge update may affect multiple apps.
  size_t expected_badge_change_count_ = 0;

  // Records a badge update for an app.
  struct BadgeChange {
    bool was_cleared_ = false;
    bool was_flagged_ = false;
    std::optional<uint64_t> last_badge_content_ = std::nullopt;
  };

  // Records a single badge update for multiple apps.
  std::unordered_map<webapps::AppId, BadgeChange> badge_change_map_;

  // Gets the recorded badge update for |app_id| from |badge_change_map_|.
  // Asserts when no recorded badge update exists for |app_id|.  Calls should be
  // wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void GetBadgeChange(const webapps::AppId& app_id, BadgeChange* result) {
    auto it = badge_change_map_.find(app_id);

    ASSERT_NE(it, badge_change_map_.end())
        << "App with ID: '" << app_id << "' did not update a badge.";

    *result = it->second;
  }

 private:
  webapps::AppId main_app_id_;
  webapps::AppId sub_app_id_;
  webapps::AppId cross_site_app_id_;
  std::unique_ptr<base::RunLoop> awaiter_;
  raw_ptr<badging::TestBadgeManagerDelegate, AcrossTasksDanglingUntriaged>
      delegate_ = nullptr;
  net::EmbeddedTestServer cross_origin_https_server_;
};

// Tests that the badge for the main frame is not affected by changing the badge
// of a cross site subframe.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest,
                       CrossSiteFrameCannotChangeMainFrameBadge) {
  // Clearing from cross site frame should affect only the cross site app.
  ExecuteScriptAndWaitForBadgeChange("navigator.clearAppBadge()",
                                     cross_site_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(cross_site_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);

  // Setting from cross site frame should affect only the cross site app.
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge(77)",
                                     cross_site_frame_);

  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(cross_site_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(77u, badge_change.last_badge_content_);
}

// Tests that setting the badge to an integer will be propagated across
// processes.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, BadgeCanBeSetToAnInteger) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge(99)", main_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::optional<uint64_t>(99u), badge_change.last_badge_content_);
}

// Tests that calls to |Badge.clear| are propagated across processes.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest,
                       BadgeCanBeClearedWithClearMethod) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge(55)", main_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::optional<uint64_t>(55u), badge_change.last_badge_content_);

  ExecuteScriptAndWaitForBadgeChange("navigator.clearAppBadge()", main_frame_);
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that calling Badge.set(0) is equivalent to calling |Badge.clear| and
// that it propagates across processes.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, BadgeCanBeClearedWithZero) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge(0)", main_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that setting the badge without content is propagated across processes.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, BadgeCanBeSetWithoutAValue) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge()", main_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that the badge can be set and cleared from an in scope frame.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest,
                       BadgeCanBeSetAndClearedFromInScopeFrame) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge()",
                                     in_scope_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);

  ExecuteScriptAndWaitForBadgeChange("navigator.clearAppBadge()",
                                     in_scope_frame_);
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that changing the badge of a subframe with an app affects the
// subframe's app.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, SubFrameBadgeAffectsSubApp) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge()", sub_app_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);

  ExecuteScriptAndWaitForBadgeChange("navigator.clearAppBadge()",
                                     sub_app_frame_);
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that setting a badge on a subframe with an app only effects the sub
// app.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, BadgeSubFrameAppViaNavigator) {
  ExecuteScriptAndWaitForBadgeChange(
      "window['sub-app'].navigator.setAppBadge()", main_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that setting a badge on a subframe via call() craziness sets the
// subframe app's badge.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, BadgeSubFrameAppViaCall) {
  ExecuteScriptAndWaitForBadgeChange(
      "const promise = "
      "  window.navigator.setAppBadge"
      "    .call(window['sub-app'].navigator);"
      "if (promise instanceof window.Promise)"
      "  throw new Error('Should be an instance of the subframes Promise!')",
      main_frame_);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Test that badging through a service worker scoped to the sub app updates
// badges for the sub app only.  These badge updates must not affect the main
// app.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest,
                       SubAppServiceWorkerBadgeAffectsSubApp) {
  const uint64_t badge_value = 1u;
  SetBadgeInServiceWorkerAndWaitForChanges(sub_app_service_worker_scope_,
                                           badge_value,
                                           /*expected_badge_change_count=*/1);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(badge_value, badge_change.last_badge_content_);

  ClearBadgeInServiceWorkerAndWaitForChanges(sub_app_service_worker_scope_,
                                             /*expected_badge_change_count=*/1);
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Test that badging through a service worker scoped to the main app updates
// badges for both the main app and the sub app.  Each service worker badge
// function call must generate 2 badge changes.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest,
                       AppServiceWorkerBadgeAffectsMultipleApps) {
  SetBadgeInServiceWorkerAndWaitForChanges(app_service_worker_scope_,
                                           std::nullopt,
                                           /*expected_badge_change_count=*/2);
  BadgeChange badge_change;
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);

  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_FALSE(badge_change.was_cleared_);
  ASSERT_TRUE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);

  ClearBadgeInServiceWorkerAndWaitForChanges(app_service_worker_scope_,
                                             /*expected_badge_change_count=*/2);
  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(main_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);

  ASSERT_NO_FATAL_FAILURE(GetBadgeChange(sub_app_id(), &badge_change));
  ASSERT_TRUE(badge_change.was_cleared_);
  ASSERT_FALSE(badge_change.was_flagged_);
  ASSERT_EQ(std::nullopt, badge_change.last_badge_content_);
}

// Tests that badging incognito windows does not cause a crash.
IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest,
                       BadgingIncognitoWindowsDoesNotCrash) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(profile(), main_frame_->GetLastCommittedURL());
  RenderFrameHost* incognito_frame = incognito_browser->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();

  ASSERT_TRUE(content::ExecJs(incognito_frame, "navigator.setAppBadge()",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(content::ExecJs(incognito_frame, "navigator.clearAppBadge()",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Updating badges through a ServiceWorkerGlobalScope must not crash.
  const std::string register_app_service_worker_script = content::JsReplace(
      kRegisterServiceWorkerScript, app_service_worker_scope_.spec());
  ASSERT_EQ("OK", EvalJs(incognito_frame, register_app_service_worker_script));

  const std::string set_badge_script = content::JsReplace(
      "postMessageToServiceWorker('$1', { command: 'set-app-badge', value: 29 "
      "});",
      app_service_worker_scope_.spec());
  ASSERT_EQ("OK", EvalJs(incognito_frame, set_badge_script));

  const std::string clear_badge_script = content::JsReplace(
      "postMessageToServiceWorker('$1', { command: 'clear-app-badge' });",
      app_service_worker_scope_.spec());
  ASSERT_EQ("OK", EvalJs(incognito_frame, clear_badge_script));
}

IN_PROC_BROWSER_TEST_F(WebAppBadgingBrowserTest, ClearLastBadgingTime) {
  ExecuteScriptAndWaitForBadgeChange("navigator.setAppBadge()", main_frame_);
  WebAppRegistrar& registrar = provider().registrar_unsafe();
  EXPECT_NE(registrar.GetAppLastBadgingTime(main_app_id()), base::Time());
  EXPECT_NE(registrar.GetAppLastLaunchTime(main_app_id()), base::Time());

  // Browsing data for all origins will be deleted.
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kPreserve);
  ChromeBrowsingDataRemoverDelegate data_remover_delegate(profile());
  base::RunLoop run_loop;
  data_remover_delegate.RemoveEmbedderData(
      /*delete_begin=*/base::Time::Min(),
      /*delete_end=*/base::Time::Max(),
      /*remove_mask=*/chrome_browsing_data_remover::DATA_TYPE_HISTORY,
      filter_builder.get(),
      /*origin_type_mask=*/1,
      base::BindLambdaForTesting([&run_loop](uint64_t failed_data_types) {
        EXPECT_EQ(failed_data_types, 0U);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(registrar.GetAppLastBadgingTime(main_app_id()), base::Time());
  EXPECT_EQ(registrar.GetAppLastLaunchTime(main_app_id()), base::Time());
}

}  // namespace web_app
