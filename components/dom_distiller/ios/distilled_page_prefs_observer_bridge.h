// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/dom_distiller/core/distilled_page_prefs.h"

// Protocol that corresponds to the DistilledPagePrefs::Observer API.
@protocol DistilledPagePrefsObserving <NSObject>
@optional
- (void)onChangeFontFamily:(dom_distiller::mojom::FontFamily)font;
- (void)onChangeTheme:(dom_distiller::mojom::Theme)theme;
- (void)onChangeFontScaling:(float)scaling;
@end

// Observer that bridges DistilledPagePrefs events to an Objective-C observer.
class DistilledPagePrefsObserverBridge
    : public dom_distiller::DistilledPagePrefs::Observer {
 public:
  explicit DistilledPagePrefsObserverBridge(
      id<DistilledPagePrefsObserving> observer);
  ~DistilledPagePrefsObserverBridge() override;

  DistilledPagePrefsObserverBridge(const DistilledPagePrefsObserverBridge&) =
      delete;
  DistilledPagePrefsObserverBridge& operator=(
      const DistilledPagePrefsObserverBridge&) = delete;

  // dom_distiller::DistilledPagePrefs::Observer implementation.
  void OnChangeFontFamily(dom_distiller::mojom::FontFamily font) override;
  void OnChangeTheme(
      dom_distiller::mojom::Theme theme,
      dom_distiller::ThemeSettingsUpdateSource source) override;
  void OnChangeFontScaling(float scaling) override;

 private:
  __weak id<DistilledPagePrefsObserving> observer_ = nil;
};

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_
