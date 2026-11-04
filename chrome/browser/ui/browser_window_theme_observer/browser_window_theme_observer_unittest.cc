// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_theme_observer/browser_window_theme_observer.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/native_theme.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;

// Minimal ThemeService that exposes NotifyThemeChanged() so tests can drive
// real ThemeServiceObserver notifications without a full theme stack.
class TestThemeService : public ThemeService {
 public:
  TestThemeService() : ThemeService(nullptr, theme_helper_) { set_ready(); }
  ~TestThemeService() override = default;

  using ThemeService::NotifyThemeChanged;

 private:
  ThemeHelper theme_helper_;
};

std::unique_ptr<KeyedService> BuildTestThemeService(
    content::BrowserContext* context) {
  return std::make_unique<TestThemeService>();
}

class BrowserWindowThemeObserverTest : public testing::Test {
 protected:
  BrowserWindowThemeObserverTest() = default;
  ~BrowserWindowThemeObserverTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ThemeServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildTestThemeService));
    profile_ = builder.Build();
    ON_CALL(mock_browser_, GetProfile()).WillByDefault(Return(profile_.get()));
  }

  TestThemeService* theme_service() {
    return static_cast<TestThemeService*>(
        ThemeServiceFactory::GetForProfile(profile_.get()));
  }

  MockBrowserWindowInterface& mock_browser() { return mock_browser_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  NiceMock<MockBrowserWindowInterface> mock_browser_;
};

TEST_F(BrowserWindowThemeObserverTest, FromReturnsInstance) {
  EXPECT_EQ(nullptr, BrowserWindowThemeObserver::From(&mock_browser()));

  {
    BrowserWindowThemeObserver observer(&mock_browser());
    EXPECT_EQ(&observer, BrowserWindowThemeObserver::From(&mock_browser()));

    const BrowserWindowInterface* const_browser = &mock_browser();
    EXPECT_EQ(&observer, BrowserWindowThemeObserver::From(const_browser));
  }

  // The observer detaches from the host on destruction.
  EXPECT_EQ(nullptr, BrowserWindowThemeObserver::From(&mock_browser()));
}

TEST_F(BrowserWindowThemeObserverTest, NotifyThemeChangedInvokesCallback) {
  BrowserWindowThemeObserver observer(&mock_browser());

  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> callback;
  base::CallbackListSubscription subscription =
      observer.RegisterThemeChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run(BrowserThemeChangeType::kWebAppTheme)).Times(1);
  observer.NotifyThemeChanged(BrowserThemeChangeType::kWebAppTheme);
}

TEST_F(BrowserWindowThemeObserverTest,
       ThemeServiceChangeNotifiesBrowserThemeType) {
  BrowserWindowThemeObserver observer(&mock_browser());

  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> callback;
  base::CallbackListSubscription subscription =
      observer.RegisterThemeChangedCallback(callback.Get());

  // A notification from the real ThemeService must be translated into a
  // kBrowserTheme change.
  EXPECT_CALL(callback, Run(BrowserThemeChangeType::kBrowserTheme)).Times(1);
  theme_service()->NotifyThemeChanged();
}

TEST_F(BrowserWindowThemeObserverTest, DestroyedSubscriptionStopsCallbacks) {
  BrowserWindowThemeObserver observer(&mock_browser());

  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> callback;
  {
    base::CallbackListSubscription subscription =
        observer.RegisterThemeChangedCallback(callback.Get());
    EXPECT_CALL(callback, Run(BrowserThemeChangeType::kBrowserTheme)).Times(1);
    observer.NotifyThemeChanged(BrowserThemeChangeType::kBrowserTheme);
  }

  // Once the RAII token is gone no further notifications are delivered.
  EXPECT_CALL(callback, Run).Times(0);
  observer.NotifyThemeChanged(BrowserThemeChangeType::kBrowserTheme);
  theme_service()->NotifyThemeChanged();
}

TEST_F(BrowserWindowThemeObserverTest, MultipleSubscribersAllNotified) {
  BrowserWindowThemeObserver observer(&mock_browser());

  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> first;
  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> second;
  base::CallbackListSubscription first_subscription =
      observer.RegisterThemeChangedCallback(first.Get());
  base::CallbackListSubscription second_subscription =
      observer.RegisterThemeChangedCallback(second.Get());

  EXPECT_CALL(first, Run(BrowserThemeChangeType::kNativeTheme)).Times(1);
  EXPECT_CALL(second, Run(BrowserThemeChangeType::kNativeTheme)).Times(1);
  observer.NotifyThemeChanged(BrowserThemeChangeType::kNativeTheme);
}

TEST_F(BrowserWindowThemeObserverTest, UnsubscribeDuringNotificationIsSafe) {
  BrowserWindowThemeObserver observer(&mock_browser());

  // A subscriber that drops its own subscription from inside the callback
  // must not crash or corrupt the callback list, and must not be invoked
  // again.
  int call_count = 0;
  base::CallbackListSubscription subscription;
  subscription = observer.RegisterThemeChangedCallback(base::BindRepeating(
      [](int* call_count, base::CallbackListSubscription* subscription,
         BrowserThemeChangeType) {
        ++*call_count;
        *subscription = {};
      },
      &call_count, &subscription));

  observer.NotifyThemeChanged(BrowserThemeChangeType::kBrowserTheme);
  observer.NotifyThemeChanged(BrowserThemeChangeType::kBrowserTheme);
  EXPECT_EQ(1, call_count);
}

// Exercises ui::NativeTheme bridging with the feature flag on and off.
class BrowserWindowThemeObserverNativeThemeTest
    : public BrowserWindowThemeObserverTest,
      public testing::WithParamInterface<bool> {
 protected:
  BrowserWindowThemeObserverNativeThemeTest() {
    feature_list_.InitWithFeatureState(
        features::kBrowserWindowListenerObservation, IsFeatureEnabled());
  }

  bool IsFeatureEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(BrowserWindowThemeObserverNativeThemeTest,
       ObservesNativeThemeOnlyWhenEnabled) {
  BrowserWindowThemeObserver observer(&mock_browser());
  EXPECT_EQ(IsFeatureEnabled(), observer.IsObservingNativeThemeForTesting());
}

TEST_P(BrowserWindowThemeObserverNativeThemeTest,
       NativeThemeUpdateNotifiesNativeThemeType) {
  BrowserWindowThemeObserver observer(&mock_browser());

  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> callback;
  base::CallbackListSubscription subscription =
      observer.RegisterThemeChangedCallback(callback.Get());

  // With the feature enabled a native theme update is translated into a
  // kNativeTheme change; with it disabled the observer must stay silent so
  // BrowserView is not notified twice (BrowserWidget still observes the native
  // theme directly).
  EXPECT_CALL(callback, Run(BrowserThemeChangeType::kNativeTheme))
      .Times(IsFeatureEnabled() ? 1 : 0);
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

TEST_P(BrowserWindowThemeObserverNativeThemeTest,
       ThemeServiceStillNotifiesRegardlessOfFlag) {
  BrowserWindowThemeObserver observer(&mock_browser());

  base::MockCallback<BrowserWindowThemeObserver::ThemeChangedCallback> callback;
  base::CallbackListSubscription subscription =
      observer.RegisterThemeChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run(BrowserThemeChangeType::kBrowserTheme)).Times(1);
  theme_service()->NotifyThemeChanged();
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserWindowThemeObserverNativeThemeTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace
