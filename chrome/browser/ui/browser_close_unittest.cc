// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingDownloadCoreService : public DownloadCoreService {
 public:
  TestingDownloadCoreService() : download_count_(0) {}

  TestingDownloadCoreService(const TestingDownloadCoreService&) = delete;
  TestingDownloadCoreService& operator=(const TestingDownloadCoreService&) =
      delete;

  ~TestingDownloadCoreService() override {}

  // All methods that aren't expected to be called in the execution of
  // this unit test are marked to result in test failure.  Using a simple
  // mock for this class should be re-evaluated if any of these
  // methods are being called; it may mean that a more fully featured
  // DownloadCoreService implementation is needed.

  void SetDownloadCount(int download_count) {
    download_count_ = download_count;
  }

  // DownloadCoreService
  ChromeDownloadManagerDelegate* GetDownloadManagerDelegate() override {
    ADD_FAILURE();
    return nullptr;
  }

  DownloadUIController* GetDownloadUIController() override {
    ADD_FAILURE();
    return nullptr;
  }

  DownloadHistory* GetDownloadHistory() override {
    ADD_FAILURE();
    return nullptr;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionDownloadsEventRouter* GetExtensionEventRouter()
      override {
    ADD_FAILURE();
    return nullptr;
  }
#endif
  bool HasCreatedDownloadManager() override { return true; }

  int BlockingShutdownCount() const override { return download_count_; }

  void CancelDownloads(DownloadCoreService::CancelDownloadsTrigger) override {}

  void SetDownloadManagerDelegateForTesting(
      std::unique_ptr<ChromeDownloadManagerDelegate> delegate) override {
    ADD_FAILURE();
  }

  bool IsDownloadUiEnabled() override { return true; }

  // KeyedService
  void Shutdown() override {}

 private:
  int download_count_;
};

static std::unique_ptr<KeyedService> CreateTestingDownloadCoreService(
    content::BrowserContext* browser_context) {
  return std::unique_ptr<KeyedService>(new TestingDownloadCoreService());
}

class BrowserCloseTest : public testing::Test {
 public:
  BrowserCloseTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()), name_index_(0) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }

  ~BrowserCloseTest() override {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override {
    for (auto& browser_window_pair : browser_windows_) {
      while (!browser_window_pair.second.empty()) {
        TestBrowserWindow* window = browser_window_pair.second.back();
        browser_window_pair.second.pop_back();
        delete window;
      }
    }
    for (auto& browser_pair : browsers_) {
      while (!browser_pair.second.empty()) {
        Browser* browser = browser_pair.second.back();
        browser_pair.second.pop_back();
        delete browser;
      }
    }
  }

  // Create a profile with the specified number of windows and downloads
  // associated with it.
  Profile* CreateProfile(int windows, int downloads) {
    std::string name(base::StringPrintf("Profile_%d", ++name_index_));
    TestingProfile* profile = profile_manager_.CreateTestingProfile(name);

    ConfigureCreatedProfile(profile, windows, downloads);

    return profile;
  }

  Profile* CreateIncognitoProfile(Profile* profile,
                                  int windows,
                                  int downloads) {
    Profile* otr_profile =
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

    ConfigureCreatedProfile(otr_profile, windows, downloads);

    return otr_profile;
  }

  Profile* CreateGuestProfile(int windows, int downloads) {
    TestingProfile* profile = profile_manager_.CreateGuestProfile();
    Profile* otr_profile =
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    ConfigureCreatedProfile(otr_profile, windows, downloads);

    return otr_profile;
  }

  Browser* GetProfileBrowser(Profile* profile, int index) {
    CHECK(browsers_.end() != browsers_.find(profile));
    CHECK_GT(browsers_[profile].size(), static_cast<size_t>(index));

    return browsers_[profile][index];
  }

 private:
  void ConfigureCreatedProfile(Profile* profile,
                               int num_windows,
                               int num_downloads) {
    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating(&CreateTestingDownloadCoreService));
    DownloadCoreService* download_core_service(
        DownloadCoreServiceFactory::GetForBrowserContext(profile));
    TestingDownloadCoreService* mock_download_service(
        static_cast<TestingDownloadCoreService*>(download_core_service));
    mock_download_service->SetDownloadCount(num_downloads);

    CHECK(browser_windows_.end() == browser_windows_.find(profile));
    CHECK(browsers_.end() == browsers_.find(profile));

    std::vector<raw_ptr<TestBrowserWindow, VectorExperimental>> windows;
    std::vector<raw_ptr<Browser, VectorExperimental>> browsers;
    for (int i = 0; i < num_windows; ++i) {
      TestBrowserWindow* window = new TestBrowserWindow();
      Browser::CreateParams params(profile, true);
      params.type = Browser::TYPE_NORMAL;
      params.window = window;
      Browser* browser = Browser::Create(params);

      windows.push_back(window);
      browsers.push_back(browser);
    }

    browser_windows_[profile] = windows;
    browsers_[profile] = browsers;
  }

  // Note that the vector elements are all owned by this class and must be
  // cleaned up.
  std::map<Profile*,
           std::vector<raw_ptr<TestBrowserWindow, VectorExperimental>>>
      browser_windows_;
  std::map<Profile*, std::vector<raw_ptr<Browser, VectorExperimental>>>
      browsers_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  int name_index_;
};

// Last window close (incognito window) will trigger warning.
TEST_F(BrowserCloseTest, LastWindowIncognito) {
  Profile* profile = CreateProfile(0, 0);
  Profile* incognito_profile = CreateIncognitoProfile(profile, 1, 1);
  Browser* browser = GetProfileBrowser(incognito_profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kBrowserShutdown,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
}

// Last incognito window close triggers incognito warning.
TEST_F(BrowserCloseTest, LastIncognito) {
  Profile* profile = CreateProfile(1, 0);
  Profile* incognito_profile = CreateIncognitoProfile(profile, 1, 1);
  Browser* browser(GetProfileBrowser(incognito_profile, 0));

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kLastWindowInIncognitoProfile,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);

  EXPECT_EQ(false, browser->CanCloseWithInProgressDownloads());
}

// Last incognito window close with no downloads => no warning.
TEST_F(BrowserCloseTest, LastIncognitoNoDownloads) {
  Profile* profile = CreateProfile(0, 0);
  Profile* incognito_profile = CreateIncognitoProfile(profile, 1, 0);
  Browser* browser = GetProfileBrowser(incognito_profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Last incognito window with window+download on another incognito profile
// => no warning.
TEST_F(BrowserCloseTest, NoIncognitoCrossChat) {
  Profile* profile1 = CreateProfile(0, 0);
  Profile* incognito_profile1 = CreateIncognitoProfile(profile1, 1, 0);
  Profile* profile2 = CreateProfile(0, 0);
  CreateIncognitoProfile(profile2, 1, 1);

  Browser* browser = GetProfileBrowser(incognito_profile1, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Non-last incognito window => no warning.
TEST_F(BrowserCloseTest, NonLastIncognito) {
  Profile* profile = CreateProfile(0, 0);
  Profile* incognito_profile = CreateIncognitoProfile(profile, 2, 1);
  Browser* browser = GetProfileBrowser(incognito_profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Non-last regular window => no warning.
TEST_F(BrowserCloseTest, NonLastRegular) {
  Profile* profile = CreateProfile(2, 1);
  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Last regular window triggers browser close warning.
TEST_F(BrowserCloseTest, LastRegular) {
  Profile* profile = CreateProfile(1, 1);
  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kBrowserShutdown,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(true, browser->CanCloseWithInProgressDownloads());
#else
  EXPECT_EQ(false, browser->CanCloseWithInProgressDownloads());
#endif
}

// Last regular window triggers browser close warning if download is on a
// different profile.
TEST_F(BrowserCloseTest, LastRegularDifferentProfile) {
  Profile* profile1 = CreateProfile(1, 0);
  CreateProfile(0, 1);

  Browser* browser = GetProfileBrowser(profile1, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kBrowserShutdown,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
}

// Last regular + incognito window + download => no warning.
TEST_F(BrowserCloseTest, LastRegularPlusIncognito) {
  Profile* profile = CreateProfile(1, 0);
  CreateIncognitoProfile(profile, 1, 1);

  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Last regular window + window on other profile => no warning.
TEST_F(BrowserCloseTest, LastRegularPlusOtherProfile) {
  Profile* profile = CreateProfile(1, 1);
  CreateProfile(1, 0);

  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Last regular window + window on other incognito profile => no warning.
TEST_F(BrowserCloseTest, LastRegularPlusOtherIncognito) {
  Profile* profile1 = CreateProfile(1, 0);
  Profile* profile2 = CreateProfile(0, 0);
  CreateIncognitoProfile(profile2, 1, 1);

  Browser* browser = GetProfileBrowser(profile1, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Last regular + download + incognito window => no warning.
TEST_F(BrowserCloseTest, LastRegularPlusIncognito2) {
  Profile* profile = CreateProfile(1, 1);
  CreateIncognitoProfile(profile, 1, 0);

  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Multiple downloads are recognized.
TEST_F(BrowserCloseTest, Plural) {
  Profile* profile = CreateProfile(1, 2);

  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kBrowserShutdown,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(2, num_downloads_blocking);
}

// Multiple downloads are recognized for incognito.
TEST_F(BrowserCloseTest, PluralIncognito) {
  Profile* profile = CreateProfile(1, 0);
  Profile* incognito_profile = CreateIncognitoProfile(profile, 1, 2);

  Browser* browser = GetProfileBrowser(incognito_profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kLastWindowInIncognitoProfile,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(2, num_downloads_blocking);
}

// Last window close (guest window) will trigger warning.
TEST_F(BrowserCloseTest, LastWindowGuest) {
  Profile* guest_profile = CreateGuestProfile(1, 1);
  Browser* browser = GetProfileBrowser(guest_profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kBrowserShutdown,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);
}

// Last guest window close triggers download warning.
TEST_F(BrowserCloseTest, LastGuest) {
  CreateProfile(1, 0);
  Profile* profile = CreateGuestProfile(1, 1);
  Browser* browser(GetProfileBrowser(profile, 0));

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kLastWindowInGuestSession,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
  EXPECT_EQ(num_downloads_blocking, 1);

  EXPECT_EQ(false, browser->CanCloseWithInProgressDownloads());
}

// Last guest window close with no downloads => no warning.
TEST_F(BrowserCloseTest, LastGuestNoDownloads) {
  Profile* profile = CreateGuestProfile(1, 0);
  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}

// Non-last guest window => no warning.
TEST_F(BrowserCloseTest, NonLastGuest) {
  Profile* profile = CreateGuestProfile(2, 1);
  Browser* browser = GetProfileBrowser(profile, 0);

  int num_downloads_blocking = 0;
  EXPECT_EQ(Browser::DownloadCloseType::kOk,
            browser->OkToCloseWithInProgressDownloads(&num_downloads_blocking));
}
