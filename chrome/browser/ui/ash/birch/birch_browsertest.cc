// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

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
    items.emplace_back(u"Cloudy", u"16 C", ui::ImageModel());
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

// Ensures the item remover is initialized, otherwise data fetches won't
// complete.
void EnsureItemRemoverInitialized() {
  BirchItemRemover* remover =
      Shell::Get()->birch_model()->GetItemRemoverForTest();
  if (!remover->Initialized()) {
    base::RunLoop run_loop;
    remover->SetProtoInitCallbackForTest(run_loop.QuitClosure());
    run_loop.Run();
  }
}

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

  void SetUp() override {
    switches::SetIgnoreForestSecretKeyForTest(true);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

  BirchKeyedService* GetBirchKeyedService() {
    return BirchKeyedServiceFactory::GetInstance()->GetService(
        browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
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

  // Disable the prefs for data providers other than weather. This ensures the
  // data is fresh once the weather provider replies.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  ASSERT_TRUE(prefs);
  prefs->SetBoolean(prefs::kBirchUseCalendar, false);
  prefs->SetBoolean(prefs::kBirchUseFileSuggest, false);
  prefs->SetBoolean(prefs::kBirchUseRecentTabs, false);
  prefs->SetBoolean(prefs::kBirchUseSelfShare, false);
  prefs->SetBoolean(prefs::kBirchUseReleaseNotes, false);

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
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto test_api = std::make_unique<OverviewGridTestApi>(root);
  EXPECT_TRUE(test_api->birch_bar_view());
  ASSERT_EQ(test_api->GetBirchChips().size(), 1u);
  BirchChipButtonBase* button = test_api->GetBirchChips()[0];
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kWeather);

  // Reset the test API to avoid danging pointers when we exit overview below.
  test_api.reset();

  // Clicking on the chip opens a browser with a Google search for weather.
  content::TestNavigationObserver navigation_observer(
      GURL("https://google.com/search?q=weather"));
  navigation_observer.StartWatchingNewWebContents();
  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));
  navigation_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(BirchBrowserTest, CalendarChip) {
  // Set up a calendar provider that provides a single calendar chip.
  auto* birch_keyed_service = GetBirchKeyedService();
  TestCalendarProvider calendar_provider;
  birch_keyed_service->set_calendar_provider_for_test(&calendar_provider);

  // Disable the prefs for data providers other than calendar. This ensures the
  // data is fresh once the calendar provider replies.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  ASSERT_TRUE(prefs);
  prefs->SetBoolean(prefs::kBirchUseFileSuggest, false);
  prefs->SetBoolean(prefs::kBirchUseRecentTabs, false);
  prefs->SetBoolean(prefs::kBirchUseSelfShare, false);
  prefs->SetBoolean(prefs::kBirchUseReleaseNotes, false);
  prefs->SetBoolean(prefs::kBirchUseWeather, false);

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
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto test_api = std::make_unique<OverviewGridTestApi>(root);
  EXPECT_TRUE(test_api->birch_bar_view());
  ASSERT_EQ(test_api->GetBirchChips().size(), 1u);
  BirchChipButtonBase* button = test_api->GetBirchChips()[0];
  EXPECT_EQ(button->GetItem()->GetType(), BirchItemType::kCalendar);

  // Reset the test API to avoid danging pointers when we exit overview below.
  test_api.reset();

  // Clicking on the chip opens a browser with a Google search for weather.
  content::TestNavigationObserver navigation_observer(
      GURL("http://example.com/"));
  navigation_observer.StartWatchingNewWebContents();
  // Clicking the button closes overview and destroys the button, so avoid a
  // dangling pointer with std::exchange.
  ClickOnView(std::exchange(button, nullptr));
  navigation_observer.Wait();
}

}  // namespace
}  // namespace ash
