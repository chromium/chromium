// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_observer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/test_profile_sync_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/startup_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

const char kContinueUrl[] = "https://www.example.com/";

class MockWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit MockWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~MockWebContentsObserver() override {}

  // A hook to verify that the OneClickSigninSyncObserver initiated a redirect
  // to the continue URL. Navigations in unit_tests never complete, but a
  // navigation start is a sufficient signal for the purposes of this test.
  // Listening for this call also has the advantage of being synchronous.
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  // TODO(jam): remove this method when PlzNavigate is turned on by default.
  MOCK_METHOD2(DidStartNavigationToPendingEntry,
               void(const GURL&, content::ReloadType));
};

class OneClickTestProfileSyncService
    : public browser_sync::TestProfileSyncService {
 public:
  ~OneClickTestProfileSyncService() override {}

  // Helper routine to be used in conjunction with
  // BrowserContextKeyedServiceFactory::SetTestingFactory().
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* profile) {
    return base::WrapUnique(new OneClickTestProfileSyncService(
        CreateProfileSyncServiceParamsForTest(
            Profile::FromBrowserContext(profile))));
  }

  bool IsFirstSetupComplete() const override { return first_setup_complete_; }

  bool IsSetupInProgress() const override { return setup_in_progress_; }

  int GetDisableReasons() const override { return DISABLE_REASON_NONE; }

  TransportState GetTransportState() const override { return state_; }

  void set_first_setup_complete(bool complete) {
    first_setup_complete_ = complete;
  }

  void set_setup_in_progress(bool in_progress) {
    setup_in_progress_ = in_progress;
  }

  void set_state(TransportState state) { state_ = state; }

 private:
  explicit OneClickTestProfileSyncService(InitParams init_params)
      : browser_sync::TestProfileSyncService(std::move(init_params)),
        first_setup_complete_(false),
        setup_in_progress_(false),
        state_(TransportState::INITIALIZING) {}

  bool first_setup_complete_;
  bool setup_in_progress_;
  TransportState state_;

  DISALLOW_COPY_AND_ASSIGN(OneClickTestProfileSyncService);
};

class TestOneClickSigninSyncObserver : public OneClickSigninSyncObserver {
 public:
  using DestructionCallback =
      base::Callback<void(TestOneClickSigninSyncObserver*)>;

  TestOneClickSigninSyncObserver(content::WebContents* web_contents,
                                 const GURL& continue_url,
                                 const DestructionCallback& callback)
      : OneClickSigninSyncObserver(web_contents, continue_url),
        destruction_callback_(callback) {}
  ~TestOneClickSigninSyncObserver() override {
    destruction_callback_.Run(this);
  }

 private:
  DestructionCallback destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestOneClickSigninSyncObserver);
};

}  // namespace

class OneClickSigninSyncObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninSyncObserverTest()
      : sync_service_(NULL),
        sync_observer_(NULL),
        sync_observer_destroyed_(true) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents_observer_.reset(new MockWebContentsObserver(web_contents()));
    sync_service_ = static_cast<OneClickTestProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&OneClickTestProfileSyncService::Build)));
  }

  void TearDown() override {
    // Verify that the |sync_observer_| unregistered as an observer from the
    // sync service and freed its memory.
    EXPECT_TRUE(sync_observer_destroyed_);
    if (sync_service_)
      EXPECT_FALSE(sync_service_->HasObserver(sync_observer_));
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void CreateSyncObserver(const std::string& url) {
    sync_observer_ = new TestOneClickSigninSyncObserver(
      web_contents(), GURL(url),
      base::Bind(&OneClickSigninSyncObserverTest::OnSyncObserverDestroyed,
                 base::Unretained(this)));
    if (sync_service_)
      EXPECT_TRUE(sync_service_->HasObserver(sync_observer_));
    EXPECT_TRUE(sync_observer_destroyed_);
    sync_observer_destroyed_ = false;
  }

  OneClickTestProfileSyncService* sync_service_;
  std::unique_ptr<MockWebContentsObserver> web_contents_observer_;

 private:
  void OnSyncObserverDestroyed(TestOneClickSigninSyncObserver* observer) {
    EXPECT_EQ(sync_observer_, observer);
    EXPECT_FALSE(sync_observer_destroyed_);
    sync_observer_destroyed_ = true;
  }

  TestOneClickSigninSyncObserver* sync_observer_;
  bool sync_observer_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncObserverTest);
};

// Verify that if no Sync service is present, e.g. because Sync is disabled, the
// observer immediately loads the continue URL.
TEST_F(OneClickSigninSyncObserverTest, NoSyncService_RedirectsImmediately) {
  // Simulate disabling Sync.
  ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  sync_service_ = static_cast<OneClickTestProfileSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile()));

  // The observer should immediately redirect to the continue URL.
  EXPECT_CALL(*web_contents_observer_, DidStartNavigation(_));
  CreateSyncObserver(kContinueUrl);
  EXPECT_EQ(GURL(kContinueUrl), web_contents()->GetVisibleURL());

  // The |sync_observer_| will be destroyed asynchronously, so manually pump
  // the message loop to wait for the destruction.
  content::RunAllPendingInMessageLoop();
}

// Verify that when the WebContents is destroyed without any Sync notifications
// firing, the observer cleans up its memory without loading the continue URL.
TEST_F(OneClickSigninSyncObserverTest, WebContentsDestroyed) {
  EXPECT_CALL(*web_contents_observer_, DidStartNavigation(_)).Times(0);
  CreateSyncObserver(kContinueUrl);
  SetContents(nullptr);
}

// Verify that when Sync is configured successfully, the observer loads the
// continue URL and cleans up after itself.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfiguredSuccessfully) {
  CreateSyncObserver(kContinueUrl);
  sync_service_->set_first_setup_complete(true);
  sync_service_->set_setup_in_progress(false);
  sync_service_->set_state(syncer::SyncService::TransportState::ACTIVE);

  EXPECT_CALL(*web_contents_observer_, DidStartNavigation(_));
  sync_service_->NotifyObservers();
  EXPECT_EQ(GURL(kContinueUrl), web_contents()->GetVisibleURL());
}

// Verify that when Sync configuration fails, the observer does not load the
// continue URL, but still cleans up after itself.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfigurationFailed) {
  CreateSyncObserver(kContinueUrl);
  sync_service_->set_first_setup_complete(true);
  sync_service_->set_setup_in_progress(false);
  sync_service_->set_state(syncer::SyncService::TransportState::INITIALIZING);

  EXPECT_CALL(*web_contents_observer_, DidStartNavigation(_)).Times(0);
  sync_service_->NotifyObservers();
  EXPECT_NE(GURL(kContinueUrl), web_contents()->GetVisibleURL());
}

// Verify that when Sync sends a notification while setup is not yet complete,
// the observer does not load the continue URL, and continues to wait.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfigurationInProgress) {
  CreateSyncObserver(kContinueUrl);
  sync_service_->set_first_setup_complete(false);
  sync_service_->set_setup_in_progress(true);
  sync_service_->set_state(syncer::SyncService::TransportState::INITIALIZING);

  EXPECT_CALL(*web_contents_observer_, DidStartNavigation(_)).Times(0);
  sync_service_->NotifyObservers();
  EXPECT_NE(GURL(kContinueUrl), web_contents()->GetVisibleURL());

  // Trigger an event to force state to be cleaned up.
  SetContents(nullptr);
}

// Verify that if the continue_url is to the settings page, no navigation is
// triggered, since it would be redundant.
TEST_F(OneClickSigninSyncObserverTest,
       OnSyncStateChanged_SyncConfiguredSuccessfully_SourceIsSettings) {
  GURL continue_url = signin::GetPromoURLForTab(
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, false);
  CreateSyncObserver(continue_url.spec());
  sync_service_->set_first_setup_complete(true);
  sync_service_->set_setup_in_progress(false);
  sync_service_->set_state(syncer::SyncService::TransportState::ACTIVE);

  EXPECT_CALL(*web_contents_observer_, DidStartNavigation(_)).Times(0);
  sync_service_->NotifyObservers();
  EXPECT_NE(GURL(kContinueUrl), web_contents()->GetVisibleURL());
}
