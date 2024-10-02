// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"
#include "chrome/browser/ui/ash/birch/birch_test_util.h"
#include "chrome/browser/ui/ash/main_extra_parts/chrome_browser_main_extra_parts_ash.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

// A weather provider that provides a single weather item.
class TestWeatherProvider : public BirchDataProvider {
 public:
  TestWeatherProvider() = default;
  ~TestWeatherProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchWeatherItem> items;
    items.emplace_back(u"Cloudy", 70.f, GURL("http://icon.com/"));
    Shell::Get()->birch_model()->SetWeatherItems(std::move(items));
  }
};

// A calendar provider that provides a single ongoing calendar event.
class TestCalendarProvider : public BirchDataProvider {
 public:
  TestCalendarProvider() = default;
  ~TestCalendarProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchCalendarItem> items;
    items.emplace_back(
        /*title=*/u"Event",
        /*start_time=*/base::Time::Now() - base::Minutes(30),
        /*end_time=*/base::Time::Now() + base::Minutes(30),
        /*calendar_url=*/GURL("http://example.com/"),
        /*conference_url=*/GURL(),
        /*event_id=*/"event_id",
        /*all_day_event=*/false);
    Shell::Get()->birch_model()->SetCalendarItems(std::move(items));

    // Also set attachments, which are normally part of calendar fetch.
    Shell::Get()->birch_model()->SetAttachmentItems({});
  }
};

// A provider that provides a single attachment.
class TestCalendarAttachmentProvider : public BirchDataProvider {
 public:
  TestCalendarAttachmentProvider() = default;
  ~TestCalendarAttachmentProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    // Also set calendar items, which are normally fetched with attachments.
    Shell::Get()->birch_model()->SetCalendarItems({});

    std::vector<BirchAttachmentItem> items;
    items.emplace_back(u"Attachment 0",
                       /*file_url=*/GURL("http://attachment.com/"),
                       /*icon_url=*/GURL("http://favicon.com/"),
                       /*start_time=*/base::Time::Now() - base::Minutes(30),
                       /*end_time=*/base::Time::Now() + base::Minutes(30),
                       /*file_id=*/"file_id0");
    Shell::Get()->birch_model()->SetAttachmentItems(std::move(items));
  }
};

// A file suggest provider that provides no files.
class EmptyFileSuggestProvider : public BirchDataProvider {
 public:
  EmptyFileSuggestProvider() = default;
  ~EmptyFileSuggestProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    Shell::Get()->birch_model()->SetFileSuggestItems({});
  }
};

// A file suggest provider that provides a single file.
class TestFileSuggestProvider : public BirchDataProvider {
 public:
  TestFileSuggestProvider() = default;
  ~TestFileSuggestProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchFileItem> items;
    items.emplace_back(base::FilePath("test-path"), "title", u"suggestion",
                       base::Time::Now() - base::Minutes(30), "file_id",
                       "icon_url");
    Shell::Get()->birch_model()->SetFileSuggestItems(std::move(items));

    // Ensure attachment data is fresh, otherwise the birch data fetch won't
    // complete. The calendar pref is disabled in the test that uses this
    // provider, so we must provide attachment data here.
    Shell::Get()->birch_model()->SetAttachmentItems({});
  }
};

// A recent tabs provider that provides a single URL.
class TestRecentTabsProvider : public BirchDataProvider {
 public:
  TestRecentTabsProvider() = default;
  ~TestRecentTabsProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchTabItem> items;
    items.emplace_back(u"tab", GURL("http://example.com/"), base::Time::Now(),
                       GURL("http://favicon.com/"), "session",
                       BirchTabItem::DeviceFormFactor::kDesktop);
    Shell::Get()->birch_model()->SetRecentTabItems(std::move(items));
  }
};

// A last active provider that provides a single URL.
class TestLastActiveProvider : public BirchDataProvider {
 public:
  TestLastActiveProvider() = default;
  ~TestLastActiveProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchLastActiveItem> items;
    items.emplace_back(u"item", GURL("http://example.com/"), base::Time());
    Shell::Get()->birch_model()->SetLastActiveItems(std::move(items));
  }
};

// A most visited provider that provides a single URL.
class TestMostVisitedProvider : public BirchDataProvider {
 public:
  TestMostVisitedProvider() = default;
  ~TestMostVisitedProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchMostVisitedItem> items;
    items.emplace_back(u"item", GURL("http://example.com/"));
    Shell::Get()->birch_model()->SetMostVisitedItems(std::move(items));
  }
};

class TestSelfShareProvider : public BirchDataProvider {
 public:
  TestSelfShareProvider() = default;
  ~TestSelfShareProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchSelfShareItem> items;
    items.emplace_back(u"guid", u"tab", GURL("http://example.com/"),
                       base::Time::Now(), u"my device",
                       SecondaryIconType::kTabFromPhone, base::DoNothing());
    Shell::Get()->birch_model()->SetSelfShareItems(std::move(items));
  }
};

class TestLostMediaProvider : public BirchDataProvider {
 public:
  TestLostMediaProvider() = default;
  ~TestLostMediaProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchLostMediaItem> items;
    items.emplace_back(GURL("https://www.source.com"), u"media title",
                       std::nullopt, SecondaryIconType::kLostMediaVideo,
                       base::BindRepeating(&TestLostMediaProvider::OnActivation,
                                           weak_factory_.GetWeakPtr()));
    Shell::Get()->birch_model()->SetLostMediaItems(std::move(items));
  }

  // Activation callback for the test lost media item.
  void OnActivation() { did_activate_ = true; }

  bool did_activate_ = false;
  base::WeakPtrFactory<TestLostMediaProvider> weak_factory_{this};
};

class TestReleaseNotesProvider : public BirchDataProvider {
 public:
  TestReleaseNotesProvider() = default;
  ~TestReleaseNotesProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    std::vector<BirchReleaseNotesItem> items;
    items.emplace_back(u"title", u"subtitle", GURL("chrome://help-app/updates"),
                       base::Time::Min());
    Shell::Get()->birch_model()->SetReleaseNotesItems(std::move(items));
  }
};

// A new window delegate that records opened files and URLs.
class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MockNewWindowDelegate() = default;
  ~MockNewWindowDelegate() override = default;

  // NewWindowDelegate:
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override {
    opened_url_ = url;
  }

  void OpenFile(const base::FilePath& file_path) override {
    opened_file_ = file_path;
  }

  GURL opened_url_;
  base::FilePath opened_file_;
};

void ClickOnView(views::View* target_view) {
  ui::test::EventGenerator event_generator(
      target_view->GetWidget()->GetNativeWindow()->GetRootWindow());
  target_view->GetWidget()->LayoutRootViewIfNecessary();
  event_generator.MoveMouseTo(target_view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

class BirchBrowserTest : public InProcessBrowserTest {
 public:
  BirchBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
  }
  ~BirchBrowserTest() override = default;
  BirchBrowserTest(const BirchBrowserTest&) = delete;
  BirchBrowserTest& operator=(const BirchBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Clear out the existing ChromeNewWindowDelegateClient so we can replace it
    // with a mock (there are CHECKs that prevent doing this without the reset).
    ChromeBrowserMainExtraPartsAsh::Get()
        ->ResetChromeNewWindowClientForTesting();
    new_window_delegate_ = std::make_unique<MockNewWindowDelegate>();
  }

  void TearDownOnMainThread() override {
    new_window_delegate_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BirchKeyedService* GetBirchKeyedService() {
    return BirchKeyedServiceFactory::GetInstance()->GetService(
        browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockNewWindowDelegate> new_window_delegate_;
};

// Test that no crash occurs on shutdown with an instantiated BirchKeyedService.
IN_PROC_BROWSER_TEST_F(BirchBrowserTest, NoCrashOnShutdown) {
  auto* birch_keyed_service = GetBirchKeyedService();
  EXPECT_TRUE(birch_keyed_service);
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, WeatherChip) {
  // Set up a weather provider so a single chip for weather will be created.
  Shell::Get()->birch_model()->OverrideWeatherProviderForTest(
      std::make_unique<TestWeatherProvider>());

  // Force it to be morning so the weather item will be ranked and appear.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kBirchIsMorning);

  // Disable the prefs for data providers other than weather. This ensures the
  // data is fresh once the weather provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseWeather});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single weather chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kWeather);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking on the chip opens a browser with a Google search for weather.
  EXPECT_EQ(new_window_delegate_->opened_url_,
            GURL("https://google.com/search?q=weather"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, CalendarChip) {
  // Set up a calendar provider that provides a single calendar chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestCalendarProvider calendar_provider;
  birch_keyed_service->set_calendar_provider_for_test(&calendar_provider);

  // Disable the prefs for data providers other than calendar. This ensures the
  // data is fresh once the calendar provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseCalendar});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single calendar chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kCalendar);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  EXPECT_EQ(new_window_delegate_->opened_url_, GURL("http://example.com/"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, AttachmentChip) {
  // Set up a calendar provider that provides a single attachment.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestCalendarAttachmentProvider attachment_provider;
  birch_keyed_service->set_calendar_provider_for_test(&attachment_provider);

  // The file suggest pref must be enabled to fetch attachments, so we must also
  // have a file suggest provider.
  EmptyFileSuggestProvider file_suggest_provider;
  birch_keyed_service->set_file_suggest_provider_for_test(
      &file_suggest_provider);

  // Disable the prefs for data providers other than file suggest, which
  // controls attachments. This ensures the data is fresh once the attachment
  // provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseFileSuggest});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single attachment chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kAttachment);

  // Clicking the button closes overview and destroys the button, so avoid
  // dangling pointers with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  EXPECT_EQ(new_window_delegate_->opened_url_, GURL("http://attachment.com/"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, FileSuggestChip) {
  // Set up a file suggest provider that provides a single file.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestFileSuggestProvider test_provider;
  birch_keyed_service->set_file_suggest_provider_for_test(&test_provider);

  // Disable the prefs for data providers other than file suggest. This ensures
  // the data is fresh once the calendar provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseFileSuggest});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single file chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kFile);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should attempt to open the file.
  EXPECT_EQ(new_window_delegate_->opened_file_, base::FilePath("test-path"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, RecentTabsChip) {
  // Set up a provider that provides a single chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestRecentTabsProvider recent_tabs_provider;
  birch_keyed_service->set_recent_tabs_provider_for_test(&recent_tabs_provider);

  // Disable the prefs for data providers other than chrome tabs. This ensures
  // the data is fresh once the test provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseChromeTabs});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kTab);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should attempt to open the URL.
  EXPECT_EQ(new_window_delegate_->opened_url_, GURL("http://example.com/"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, LastActiveChip) {
  // Set up a last active provider that provides a single chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestLastActiveProvider last_active_provider;
  birch_keyed_service->set_last_active_provider_for_test(&last_active_provider);

  // Last active chips only show in the morning so force morning in the ranker.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kBirchIsMorning);

  // Disable the prefs for data providers other than chrome tabs. This ensures
  // the data is fresh once the last active provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseChromeTabs});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kLastActive);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should attempt to open the URL.
  EXPECT_EQ(new_window_delegate_->opened_url_, GURL("http://example.com/"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, MostVisitedChip) {
  // Set up a most visited provider that provides a single chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestMostVisitedProvider most_visited_provider;
  birch_keyed_service->set_most_visited_provider_for_test(
      &most_visited_provider);

  // Most visited chips only show in the morning so force morning in the ranker.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kBirchIsMorning);

  // Disable the prefs for data providers other than chrome tabs. This ensures
  // the data is fresh once the most visited provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseChromeTabs});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kMostVisited);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should attempt to open the URL.
  EXPECT_EQ(new_window_delegate_->opened_url_, GURL("http://example.com/"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, SelfShareChip) {
  // Set up a self share provider that provides a single chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestSelfShareProvider self_share_provider;
  birch_keyed_service->set_self_share_provider_for_test(&self_share_provider);

  // Disable the prefs for data providers other than chrome tabs. This ensures
  // the data is fresh once the self share provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseChromeTabs});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kSelfShare);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should attempt to open the URL.
  EXPECT_EQ(new_window_delegate_->opened_url_, GURL("http://example.com/"));
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, LostMediaChip) {
  // Set up a provider that provides a single chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestLostMediaProvider lost_media_provider;
  birch_keyed_service->set_lost_media_provider_for_test(&lost_media_provider);

  // Disable the prefs for data providers other than lost media. This ensures
  // the data is fresh once the lost media provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseLostMedia});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kLostMedia);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should activate the item. In this test the activation
  // callback is bound to the provider.
  EXPECT_TRUE(lost_media_provider.did_activate_);

  // Reset lost media provider.
  birch_keyed_service->set_lost_media_provider_for_test(nullptr);
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, ReleaseNotesChip) {
  // Set up a provider that provides a single chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestReleaseNotesProvider provider;
  birch_keyed_service->set_release_notes_provider_for_test(&provider);

  // Disable the prefs for data providers other than release notes. This
  // ensures the data is fresh once the release notes provider replies.
  DisableAllDataTypePrefsExcept({prefs::kBirchUseReleaseNotes});

  // Ensure the item remover is initialized, otherwise data fetches won't
  // complete.
  EnsureItemRemoverInitialized();

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Enter overview, which triggers a birch data fetch.
  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* button = GetBirchChipButton();
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kReleaseNotes);

  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));

  // Clicking the button should attempt to open the URL.
  EXPECT_EQ(new_window_delegate_->opened_url_,
            GURL("chrome://help-app/updates"));
}

}  // namespace
}  // namespace ash
