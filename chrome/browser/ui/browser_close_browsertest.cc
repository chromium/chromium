// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/mock_download_core_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

class BrowserCloseTest : public InProcessBrowserTest {
 public:
  BrowserCloseTest() = default;
  ~BrowserCloseTest() override = default;

  std::unique_ptr<KeyedService> CreateMockDownloadCoreService(
      content::BrowserContext* browser_context) {
    Profile* profile = static_cast<Profile*>(browser_context);
    auto mock = std::make_unique<testing::NiceMock<MockDownloadCoreService>>();

    auto delegate = std::make_unique<ChromeDownloadManagerDelegate>(profile);
    ChromeDownloadManagerDelegate* delegate_ptr = delegate.get();
    delegates_[profile] = std::move(delegate);

    ON_CALL(*mock, HasCreatedDownloadManager())
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock, IsDownloadUiEnabled()).WillByDefault(testing::Return(true));
    ON_CALL(*mock, GetDownloadManagerDelegate())
        .WillByDefault(testing::Return(delegate_ptr));
    ON_CALL(*mock, Shutdown())
        .WillByDefault([this, profile, delegate_ptr]() {
          if (profile) {
            profile->GetDownloadManager()->SetDelegate(nullptr);
          }
          delegate_ptr->Shutdown();
          delegates_.erase(profile);
        });
    return mock;
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // This test dynamically creates new profiles for testing multi-profile
    // download warnings without registering corresponding user manager users.
    // This switch tells ProfileHelper to fall back to the active user during
    // profile creation instead of crashing due to missing user manager mapping.
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
  }
#endif

  void TearDownOnMainThread() override {
    // Reset download counts on all mocked profiles to 0 first so they don't
    // block any closures
    for (auto& pair : mock_services_) {
      ON_CALL(*pair.second, BlockingShutdownCount())
          .WillByDefault(testing::Return(0));
    }

    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Track mocked download core services
  std::map<Profile*, MockDownloadCoreService*> mock_services_;
  std::map<Profile*, std::unique_ptr<ChromeDownloadManagerDelegate>> delegates_;

  // Helper to dynamically create a real profile
  Profile* CreateProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath path = profile_manager->GenerateNextProfileDirectoryPath();
    return &profiles::testing::CreateProfileSync(profile_manager, path);
  }

  // Helper to configure mocked download count on a profile
  void MockDownloadCount(Profile* profile, int downloads) {
    auto it = mock_services_.find(profile);
    MockDownloadCoreService* mock_service = nullptr;
    if (it == mock_services_.end()) {
      DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
          profile,
          base::BindRepeating(&BrowserCloseTest::CreateMockDownloadCoreService,
                              base::Unretained(this)));
      DownloadCoreService* download_core_service =
          DownloadCoreServiceFactory::GetForBrowserContext(profile);
      mock_service =
          static_cast<MockDownloadCoreService*>(download_core_service);
      mock_services_[profile] = mock_service;
    } else {
      mock_service = it->second;
    }
    ON_CALL(*mock_service, BlockingShutdownCount())
        .WillByDefault(testing::Return(downloads));
  }
};

// Last window close (incognito window) will trigger warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastWindowIncognito) {
  Profile* profile = CreateProfile();
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  MockDownloadCount(incognito_browser->GetProfile(), 1);
  CloseBrowserSynchronously(browser());

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kBrowserShutdown,
            UnloadController::From(incognito_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
}

// Last incognito window close triggers incognito warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->GetProfile());
  MockDownloadCount(incognito_browser->GetProfile(), 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kLastWindowInIncognitoProfile,
            UnloadController::From(incognito_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);

  EXPECT_EQ(false, UnloadController::From(incognito_browser)
                       ->CanCloseWithInProgressDownloads());
}

// Last incognito window close with no downloads => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastIncognitoNoDownloads) {
  Profile* profile = CreateProfile();
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  MockDownloadCount(incognito_browser->GetProfile(), 0);
  CloseBrowserSynchronously(browser());

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(incognito_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Last incognito window with window+download on another incognito profile
// => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, NoIncognitoCrossChat) {
  Profile* profile1 = CreateProfile();
  Browser* incognito_browser1 = CreateIncognitoBrowser(profile1);
  MockDownloadCount(incognito_browser1->GetProfile(), 0);

  Profile* profile2 = CreateProfile();
  Browser* incognito_browser2 = CreateIncognitoBrowser(profile2);
  MockDownloadCount(incognito_browser2->GetProfile(), 1);

  CloseBrowserSynchronously(browser());

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(incognito_browser1)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Non-last incognito window => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, NonLastIncognito) {
  Profile* profile = CreateProfile();
  Browser* incognito_browser1 = CreateIncognitoBrowser(profile);
  CreateIncognitoBrowser(profile);
  MockDownloadCount(incognito_browser1->GetProfile(), 1);

  CloseBrowserSynchronously(browser());

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(incognito_browser1)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Non-last regular window => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, NonLastRegular) {
  Profile* profile = browser()->profile();
  CreateBrowser(profile);
  MockDownloadCount(profile, 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
}

// Last regular window triggers browser close warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastRegular) {
  MockDownloadCount(browser()->GetProfile(), 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kBrowserShutdown,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(
      true,
      UnloadController::From(browser())->CanCloseWithInProgressDownloads());
#else
  EXPECT_EQ(
      false,
      UnloadController::From(browser())->CanCloseWithInProgressDownloads());
#endif
}

// Last regular window triggers browser close warning if download is on a
// different profile.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastRegularDifferentProfile) {
  MockDownloadCount(browser()->GetProfile(), 0);

  Profile* profile2 = CreateProfile();
  MockDownloadCount(profile2, 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kBrowserShutdown,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
}

// Last regular + incognito window + download => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastRegularPlusIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->GetProfile());
  MockDownloadCount(incognito_browser->GetProfile(), 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
}

// Last regular window + window on other profile => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastRegularPlusOtherProfile) {
  MockDownloadCount(browser()->GetProfile(), 1);

  Profile* profile2 = CreateProfile();
  CreateBrowser(profile2);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
}

// Last regular window + window on other incognito profile => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastRegularPlusOtherIncognito) {
  MockDownloadCount(browser()->GetProfile(), 0);

  Profile* profile2 = CreateProfile();
  Browser* incognito_browser2 = CreateIncognitoBrowser(profile2);
  MockDownloadCount(incognito_browser2->GetProfile(), 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
}

// Last regular + download + incognito window => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastRegularPlusIncognito2) {
  MockDownloadCount(browser()->GetProfile(), 1);

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->GetProfile());
  MockDownloadCount(incognito_browser->GetProfile(), 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
}

// Multiple downloads are recognized.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, Plural) {
  MockDownloadCount(browser()->GetProfile(), 2);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kBrowserShutdown,
            UnloadController::From(browser())->OkToCloseWithInProgressDownloads(
                &num_downloads_blocking));
  EXPECT_EQ(2, num_downloads_blocking);
}

// Multiple downloads are recognized for incognito.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, PluralIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->GetProfile());
  MockDownloadCount(incognito_browser->GetProfile(), 2);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kLastWindowInIncognitoProfile,
            UnloadController::From(incognito_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(2, num_downloads_blocking);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Last window close (guest window) will trigger warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastWindowGuest) {
  Browser* guest_browser = CreateGuestBrowser();
  MockDownloadCount(guest_browser->GetProfile(), 1);
  CloseBrowserSynchronously(browser());

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kBrowserShutdown,
            UnloadController::From(guest_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
}

// Last guest window close triggers download warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastGuest) {
  Browser* guest_browser = CreateGuestBrowser();
  MockDownloadCount(guest_browser->GetProfile(), 1);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kLastWindowInGuestSession,
            UnloadController::From(guest_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);

  EXPECT_EQ(
      false,
      UnloadController::From(guest_browser)->CanCloseWithInProgressDownloads());
}

// Last guest window close with no downloads => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, LastGuestNoDownloads) {
  Browser* guest_browser = CreateGuestBrowser();
  MockDownloadCount(guest_browser->GetProfile(), 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(guest_browser)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Non-last guest window => no warning.
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, NonLastGuest) {
  Browser* guest_browser1 = CreateGuestBrowser();
  CreateGuestBrowser();
  MockDownloadCount(guest_browser1->GetProfile(), 1);

  CloseBrowserSynchronously(browser());

  int num_downloads_blocking = 0;
  EXPECT_EQ(UnloadController::DownloadCloseType::kOk,
            UnloadController::From(guest_browser1)
                ->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
