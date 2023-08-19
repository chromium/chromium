// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/ui_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "components/safe_browsing/content/browser/safe_browsing_controller_client.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

static const char* kGoodURL = "https://www.good.com";
static const char* kBadURL = "https://www.malware.com";
static const char* kBadURLWithPath = "https://www.malware.com/index.html";
static const char* kAnotherBadURL = "https://www.badware.com";
static const char* kLandingURL = "https://www.landing.com";

namespace safe_browsing {

class SafeBrowsingCallbackWaiter {
 public:
  SafeBrowsingCallbackWaiter() {}

  bool callback_called() const { return callback_called_; }
  bool proceed() const { return proceed_; }
  bool showed_interstitial() const { return showed_interstitial_; }

  void OnBlockingPageDone(bool proceed, bool showed_interstitial) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    callback_called_ = true;
    proceed_ = proceed;
    showed_interstitial_ = showed_interstitial;
    loop_.Quit();
  }

  void OnBlockingPageDoneOnIO(bool proceed, bool showed_interstitial) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                       base::Unretained(this), proceed, showed_interstitial));
  }

  void WaitForCallback() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    loop_.Run();
  }

 private:
  bool callback_called_ = false;
  bool proceed_ = false;
  bool showed_interstitial_ = false;
  base::RunLoop loop_;
};

// A test blocking page that does not create windows.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(BaseUIManager* manager,
                               content::WebContents* web_contents,
                               const GURL& main_frame_url,
                               const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(
            manager,
            web_contents,
            main_frame_url,
            unsafe_resources,
            std::make_unique<safe_browsing::SafeBrowsingControllerClient>(
                web_contents,
                std::make_unique<security_interstitials::MetricsHelper>(
                    unsafe_resources[0].url,
                    BaseBlockingPage::GetReportingInfo(unsafe_resources),
                    /*history_service=*/nullptr),
                /*prefs=*/nullptr,
                manager->app_locale(),
                manager->default_safe_page(),
                /*settings_helper=*/nullptr),
            BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
                BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
                false,                 // is_extended_reporting_opt_in_allowed
                false,                 // is_off_the_record
                false,                 // is_extended_reporting_enabled
                false,                 // is_extended_reporting_policy_managed
                false,                 // is_enhanced_protection_enabled
                false,                 // is_proceed_anyway_disabled
                true,                  // should_open_links_in_new_tab
                true,                  // always_show_back_to_safety
                false,                 // is_enhanced_protection_message_enabled
                false,                 // is_safe_browsing_managed
                "cpn_safe_browsing"),  // help_center_article_link
            true,                      // should_trigger_reporting
            /*history_service=*/nullptr,
            /*navigation_observer_manager=*/nullptr,
            /*metrics_collector=*/nullptr,
            /*trigger_manager=*/nullptr,
            /*is_proceed_anyway_disabled=*/false,
            /*is_safe_browsing_surveys_enabled=*/true) {
    // Don't delay details at all for the unittest.
    SetThreatDetailsProceedDelayForTesting(0);
    DontCreateViewForTesting();
  }
};

// A factory that creates TestSafeBrowsingBlockingPages.
class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() = default;
  ~TestSafeBrowsingBlockingPageFactory() override = default;

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* delegate,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting) override {
    return new TestSafeBrowsingBlockingPage(delegate, web_contents,
                                            main_frame_url, unsafe_resources);
  }
#if !BUILDFLAG(IS_ANDROID)
  security_interstitials::SecurityInterstitialPage* CreateEnterpriseWarnPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    NOTREACHED();
    return nullptr;
  }

  security_interstitials::SecurityInterstitialPage* CreateEnterpriseBlockPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    NOTREACHED();
    return nullptr;
  }
#endif
};

class TestSafeBrowsingUIManagerDelegate
    : public SafeBrowsingUIManager::Delegate {
 public:
  TestSafeBrowsingUIManagerDelegate() {
    safe_browsing::RegisterProfilePrefs(pref_service_.registry());
  }

  ~TestSafeBrowsingUIManagerDelegate() override = default;

  // SafeBrowsingUIManager::Delegate:
  std::string GetApplicationLocale() override { return "en-us"; }
  void TriggerSecurityInterstitialShownExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code) override {}
  void TriggerSecurityInterstitialProceededExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code) override {}
#if !BUILDFLAG(IS_ANDROID)
  void TriggerUrlFilteringInterstitialExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& threat_type,
      safe_browsing::RTLookupResponse rt_lookup_response) override {}
#endif
  prerender::NoStatePrefetchContents* GetNoStatePrefetchContentsIfExists(
      content::WebContents* web_contents) override {
    return nullptr;
  }
  bool IsHostingExtension(content::WebContents* web_contents) override {
    return is_hosting_extension_;
  }
  PrefService* GetPrefs(content::BrowserContext* browser_context) override {
    return &pref_service_;
  }
  history::HistoryService* GetHistoryService(
      content::BrowserContext* browser_context) override {
    return nullptr;
  }
  PingManager* GetPingManager(
      content::BrowserContext* browser_context) override {
    return nullptr;
  }
  bool IsMetricsAndCrashReportingEnabled() override { return false; }

  bool IsSendingOfHitReportsEnabled() override { return false; }

  void set_is_hosting_extension(bool is_hosting_extension) {
    is_hosting_extension_ = is_hosting_extension;
  }

 private:
  bool is_hosting_extension_ = false;
  TestingPrefServiceSimple pref_service_;
};

class SafeBrowsingUIManagerTest : public content::RenderViewHostTestHarness {
 public:
  SafeBrowsingUIManagerTest() {
    auto ui_manager_delegate =
        std::make_unique<TestSafeBrowsingUIManagerDelegate>();
    raw_ui_manager_delegate_ = ui_manager_delegate.get();
    ui_manager_ = new SafeBrowsingUIManager(
        std::move(ui_manager_delegate),
        std::make_unique<TestSafeBrowsingBlockingPageFactory>(),
        GURL("chrome://new-tab-page/"));
  }

  ~SafeBrowsingUIManagerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SafeBrowsingUIManager::CreateAllowlistForTesting(web_contents());
  }

  bool IsAllowlisted(security_interstitials::UnsafeResource resource) {
    return ui_manager_->IsAllowlisted(resource);
  }

  void AddToAllowlist(security_interstitials::UnsafeResource resource) {
    ui_manager_->AddToAllowlistUrlSet(
        SafeBrowsingUIManager::GetMainFrameAllowlistUrlForResourceForTesting(
            resource),
        web_contents(), false, resource.threat_type);
  }

  security_interstitials::UnsafeResource MakeUnsafeResource(
      const char* url,
      bool is_subresource,
      // TODO(crbug.com/1410253): Delete parameter once the experiment is
      // complete.
      absl::optional<content::GlobalRenderFrameHostId>
          override_primary_main_frame_id = absl::nullopt) {
    const content::GlobalRenderFrameHostId primary_main_frame_id =
        override_primary_main_frame_id.has_value()
            ? override_primary_main_frame_id.value()
            : web_contents()->GetPrimaryMainFrame()->GetGlobalId();
    security_interstitials::UnsafeResource resource;
    resource.url = GURL(url);
    resource.is_subresource = is_subresource;
    resource.render_process_id = primary_main_frame_id.child_id;
    resource.render_frame_id = primary_main_frame_id.frame_routing_id;
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    return resource;
  }

  security_interstitials::UnsafeResource MakeUnsafeResourceAndStartNavigation(
      const char* url) {
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(url, false /* is_subresource */);

    // The WC doesn't have a URL without a navigation. A main-frame malware
    // unsafe resource must be a pending navigation.
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(url), web_contents());
    navigation->Start();
    return resource;
  }

  void SimulateBlockingPageDone(
      const std::vector<security_interstitials::UnsafeResource>& resources,
      bool proceed) {
    GURL main_frame_url;
    content::NavigationEntry* entry =
        web_contents()->GetController().GetVisibleEntry();
    if (entry)
      main_frame_url = entry->GetURL();

    ui_manager_->OnBlockingPageDone(resources, proceed, web_contents(),
                                    main_frame_url,
                                    true /* showed_interstitial */);
  }

 protected:
  SafeBrowsingUIManager* ui_manager() { return ui_manager_.get(); }
  TestSafeBrowsingUIManagerDelegate* ui_manager_delegate() {
    return raw_ui_manager_delegate_;
  }

 private:
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  raw_ptr<TestSafeBrowsingUIManagerDelegate> raw_ui_manager_delegate_ = nullptr;
};

TEST_F(SafeBrowsingUIManagerTest, Allowlist) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToAllowlist(resource);
  EXPECT_TRUE(IsAllowlisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, AllowlistIgnoresSitesNotAdded) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kGoodURL);
  EXPECT_FALSE(IsAllowlisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, AllowlistRemembersThreatType) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToAllowlist(resource);
  EXPECT_TRUE(IsAllowlisted(resource));
  SBThreatType threat_type;
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(ui_manager()->IsUrlAllowlistedOrPendingForWebContents(
      resource.url, resource.is_subresource, entry,
      security_interstitials::GetWebContentsForResource(resource), true,
      &threat_type));
  EXPECT_EQ(resource.threat_type, threat_type);
}

TEST_F(SafeBrowsingUIManagerTest, AllowlistIgnoresPath) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToAllowlist(resource);
  EXPECT_TRUE(IsAllowlisted(resource));

  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  security_interstitials::UnsafeResource resource_path =
      MakeUnsafeResourceAndStartNavigation(kBadURLWithPath);
  EXPECT_TRUE(IsAllowlisted(resource_path));
}

TEST_F(SafeBrowsingUIManagerTest, AllowlistIgnoresThreatType) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToAllowlist(resource);
  EXPECT_TRUE(IsAllowlisted(resource));

  security_interstitials::UnsafeResource resource_phishing =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);
  resource_phishing.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  EXPECT_TRUE(IsAllowlisted(resource_phishing));
}

TEST_F(SafeBrowsingUIManagerTest, AllowlistWithUnrelatedPendingLoad) {
  // Commit load of landing page.
  NavigateAndCommit(GURL(kLandingURL));
  auto unrelated_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(GURL(kGoodURL),
                                                           web_contents());
  {
    // Simulate subresource malware hit on the landing page.
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);

    // Start pending load to unrelated site.
    unrelated_navigation->Start();

    // Allowlist the resource on the landing page.
    AddToAllowlist(resource);
    EXPECT_TRUE(IsAllowlisted(resource));
  }

  // Commit the pending load of unrelated site.
  unrelated_navigation->Commit();
  {
    // The unrelated site is not on the allowlist, even if the same subresource
    // was on it.
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);
    EXPECT_FALSE(IsAllowlisted(resource));
  }

  // Navigate back to the original landing url.
  NavigateAndCommit(GURL(kLandingURL));
  {
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);
    // Original resource url is allowlisted.
    EXPECT_TRUE(IsAllowlisted(resource));
  }
  {
    // A different malware subresource on the same page is also allowlisted.
    // (The allowlist is by the page url, not the resource url.)
    security_interstitials::UnsafeResource resource2 =
        MakeUnsafeResource(kAnotherBadURL, true /* is_subresource */);
    EXPECT_TRUE(IsAllowlisted(resource2));
  }
}

TEST_F(SafeBrowsingUIManagerTest, UICallbackProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetUIThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(IsAllowlisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
}

TEST_F(SafeBrowsingUIManagerTest, UICallbackDontProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetUIThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsAllowlisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

TEST_F(SafeBrowsingUIManagerTest, IOCallbackProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetIOThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(IsAllowlisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
}

TEST_F(SafeBrowsingUIManagerTest, IOCallbackDontProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetIOThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsAllowlisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

namespace {

// A WebContentsDelegate that records whether
// VisibleSecurityStateChanged() was called.
class SecurityStateWebContentsDelegate : public content::WebContentsDelegate {
 public:
  SecurityStateWebContentsDelegate() {}

  SecurityStateWebContentsDelegate(const SecurityStateWebContentsDelegate&) =
      delete;
  SecurityStateWebContentsDelegate& operator=(
      const SecurityStateWebContentsDelegate&) = delete;

  ~SecurityStateWebContentsDelegate() override {}

  bool visible_security_state_changed() const {
    return visible_security_state_changed_;
  }

  void ClearVisibleSecurityStateChanged() {
    visible_security_state_changed_ = false;
  }

  // WebContentsDelegate:
  void VisibleSecurityStateChanged(content::WebContents* source) override {
    visible_security_state_changed_ = true;
  }

 private:
  bool visible_security_state_changed_ = false;
};

}  // namespace

// Tests that the WebContentsDelegate is notified of a visible security
// state change when a blocking page is shown for a subresource.
TEST_F(SafeBrowsingUIManagerTest,
       VisibleSecurityStateChangedForUnsafeSubresource) {
  SecurityStateWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  // Simulate a blocking page showing for an unsafe subresource.
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, true /* is_subresource */);
  // Needed for showing the blocking page.
  resource.threat_source = safe_browsing::ThreatSource::REMOTE;

  NavigateAndCommit(GURL("http://example.test"));

  delegate.ClearVisibleSecurityStateChanged();
  EXPECT_FALSE(delegate.visible_security_state_changed());
  ui_manager()->DisplayBlockingPage(resource);
  EXPECT_TRUE(delegate.visible_security_state_changed());

  // Simulate proceeding through the blocking page.
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetIOThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);

  delegate.ClearVisibleSecurityStateChanged();
  EXPECT_FALSE(delegate.visible_security_state_changed());
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(delegate.visible_security_state_changed());

  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
  EXPECT_TRUE(IsAllowlisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, ShowBlockPageNoCallback) {
  SecurityStateWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  // Simulate a blocking page showing for an unsafe subresource.
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);
  // Needed for showing the blocking page.
  resource.threat_source = safe_browsing::ThreatSource::REMOTE;

  // This call caused a crash in https://crbug.com/1058094. Just verify that we
  // don't crash anymore.
  ui_manager()->DisplayBlockingPage(resource);
}

TEST_F(SafeBrowsingUIManagerTest, NoInterstitialInExtensions) {
  // Pretend the current web contents is in an extension.
  ui_manager_delegate()->set_is_hosting_extension(true);

  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);

  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetUIThreadTaskRunner({});
  ui_manager()->StartDisplayingBlockingPage(resource);
  waiter.WaitForCallback();
  EXPECT_FALSE(waiter.proceed());
  EXPECT_FALSE(waiter.showed_interstitial());
}

TEST_F(SafeBrowsingUIManagerTest, DisplayInterstitial) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);

  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetUIThreadTaskRunner({});
  ui_manager()->StartDisplayingBlockingPage(resource);
  waiter.WaitForCallback();
  EXPECT_FALSE(waiter.proceed());
  EXPECT_TRUE(waiter.showed_interstitial());
}

// Same as |DisplayInterstitial| but within the Safe Browsing lookup mechanism
// experiment, which calls |CheckExperimentEligibilityAndStartBlockingPage|
// instead of |StartDisplayingBlockingPage|.
TEST_F(SafeBrowsingUIManagerTest,
       LookupMechanismExperiment_DisplayInterstitial) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, /*is_subresource=*/false);

  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::BindRepeating(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                          base::Unretained(&waiter));
  resource.callback_sequence = content::GetUIThreadTaskRunner({});
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(/*is_eligible=*/true));
  ui_manager()->CheckExperimentEligibilityAndStartBlockingPage(
      resource, callback.Get(), base::SequencedTaskRunner::GetCurrentDefault());
  waiter.WaitForCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(waiter.proceed());
  EXPECT_TRUE(waiter.showed_interstitial());
}

TEST_F(SafeBrowsingUIManagerTest, CheckLookupMechanismExperimentEligibility) {
  auto run_test = [this](
                      bool expect_is_eligible,
                      const security_interstitials::UnsafeResource& resource) {
    base::MockCallback<base::OnceCallback<void(bool)>> callback;
    EXPECT_CALL(callback, Run(expect_is_eligible));
    ui_manager()->CheckLookupMechanismExperimentEligibility(
        resource, callback.Get(),
        base::SequencedTaskRunner::GetCurrentDefault());
    base::RunLoop().RunUntilIdle();
  };
  {
    // Eligible: Default configuration.
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, /*is_subresource=*/false);
    run_test(/*expect_is_eligible=*/true, /*resource=*/resource);
  }
  {
    // Ineligible: Allowlisted URL.
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, /*is_subresource=*/false);
    AddToAllowlist(resource);
    run_test(/*expect_is_eligible=*/false, /*resource=*/resource);
  }
  {
    // Ineligible: Hosting extension.
    ui_manager_delegate()->set_is_hosting_extension(true);
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, /*is_subresource=*/false);
    run_test(/*expect_is_eligible=*/false, /*resource=*/resource);
  }
  {
    // Ineligible: No web contents.
    const content::GlobalRenderFrameHostId primary_main_frame_id =
        web_contents()->GetPrimaryMainFrame()->GetGlobalId();
    DeleteContents();
    security_interstitials::UnsafeResource resource = MakeUnsafeResource(
        kBadURL, /*is_subresource=*/false,
        /*override_primary_main_frame_id=*/primary_main_frame_id);
    run_test(/*expect_is_eligible=*/false, /*resource=*/resource);
  }
}

TEST_F(SafeBrowsingUIManagerTest, InvalidRenderFrameHostId) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);

  // Clobber the resource's RenderFrameHost id so that subsequent WebContents
  // lookups return null. This simulates the destruction of a RenderFrameHost
  // (for the purposes of a WebContents lookup) which SafeBrowsing needs to
  // handle.
  content::GlobalRenderFrameHostId invalid_rfh_id;
  resource.render_process_id = invalid_rfh_id.child_id;
  resource.render_frame_id = invalid_rfh_id.frame_routing_id;
  ASSERT_FALSE(security_interstitials::GetWebContentsForResource(resource));

  EXPECT_FALSE(IsAllowlisted(resource));
}

}  // namespace safe_browsing
