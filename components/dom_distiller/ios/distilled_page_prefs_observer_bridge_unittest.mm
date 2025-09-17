// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/dom_distiller/ios/distilled_page_prefs_observer_bridge.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeDistilledPagePrefsObserver
    : NSObject <DistilledPagePrefsObserving>
@property(nonatomic, assign)
    dom_distiller::mojom::FontFamily notifiedFontFamily;
@property(nonatomic, assign) dom_distiller::mojom::Theme notifiedTheme;
@property(nonatomic, assign) float notifiedFontScaling;
@end

@implementation FakeDistilledPagePrefsObserver
- (void)onChangeFontFamily:(dom_distiller::mojom::FontFamily)font {
  self.notifiedFontFamily = font;
}

- (void)onChangeTheme:(dom_distiller::mojom::Theme)theme {
  self.notifiedTheme = theme;
}

- (void)onChangeFontScaling:(float)scaling {
  self.notifiedFontScaling = scaling;
}
@end

// Test fixture for DistilledPagePrefsObserverBridge.
class DistilledPagePrefsObserverBridgeTest : public PlatformTest {
 public:
  DistilledPagePrefsObserverBridgeTest()
      : observer_([[FakeDistilledPagePrefsObserver alloc] init]),
        observer_bridge_(observer_) {}

 protected:
  FakeDistilledPagePrefsObserver* observer_;
  DistilledPagePrefsObserverBridge observer_bridge_;
};

// Tests that the observer is notified of font family changes.
TEST_F(DistilledPagePrefsObserverBridgeTest, OnChangeFontFamily) {
  observer_bridge_.OnChangeFontFamily(dom_distiller::mojom::FontFamily::kSerif);
  EXPECT_EQ([observer_ notifiedFontFamily],
            dom_distiller::mojom::FontFamily::kSerif);
}

// Tests that the observer is notified of user preference theme changes.
TEST_F(DistilledPagePrefsObserverBridgeTest, OnChangeUserPreferenceTheme) {
  observer_bridge_.OnChangeTheme(
      dom_distiller::mojom::Theme::kDark,
      dom_distiller::ThemeSettingsUpdateSource::kUserPreference);
  EXPECT_EQ([observer_ notifiedTheme], dom_distiller::mojom::Theme::kDark);
}

// Tests that the observer is notified of system theme changes.
TEST_F(DistilledPagePrefsObserverBridgeTest, OnChangeSystemTheme) {
  observer_bridge_.OnChangeTheme(dom_distiller::mojom::Theme::kDark,
                                 dom_distiller::ThemeSettingsUpdateSource::kSystem);
  EXPECT_EQ([observer_ notifiedTheme], dom_distiller::mojom::Theme::kDark);
}

// Tests that the observer is notified of font scaling changes.
TEST_F(DistilledPagePrefsObserverBridgeTest, OnChangeFontScaling) {
  observer_bridge_.OnChangeFontScaling(1.5f);
  EXPECT_EQ([observer_ notifiedFontScaling], 1.5f);
}
