// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_
#define COMPONENTS_WEB_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/web_distiller/core/distilled_page_prefs.h"

// Protocol that corresponds to the DistilledPagePrefs::Observer API.
@protocol DistilledPagePrefsObserving <NSObject>
@optional
- (void)onChangeFontFamily:(web_distiller::mojom::FontFamily)font;
- (void)onChangeTheme:(web_distiller::mojom::Theme)theme
           withSource:(web_distiller::ThemeSettingsUpdateSource)source;
- (void)onChangeFontScaling:(float)scaling;
- (void)onChangeLinksEnabled:(BOOL)enabled;
@end

// Observer that bridges DistilledPagePrefs events to an Objective-C observer.
class DistilledPagePrefsObserverBridge
    : public web_distiller::DistilledPagePrefs::Observer {
 public:
  DistilledPagePrefsObserverBridge(
      id<DistilledPagePrefsObserving> observer,
      web_distiller::DistilledPagePrefs* distilled_page_prefs);
  ~DistilledPagePrefsObserverBridge() override;

  DistilledPagePrefsObserverBridge(const DistilledPagePrefsObserverBridge&) =
      delete;
  DistilledPagePrefsObserverBridge& operator=(
      const DistilledPagePrefsObserverBridge&) = delete;

  // web_distiller::DistilledPagePrefs::Observer implementation.
  void OnChangeFontFamily(web_distiller::mojom::FontFamily font) override;
  void OnChangeTheme(web_distiller::mojom::Theme theme,
                     web_distiller::ThemeSettingsUpdateSource source) override;
  void OnChangeFontScaling(float scaling) override;
  void OnChangeLinksEnabled(bool enabled) override;

 private:
  __weak id<DistilledPagePrefsObserving> observer_ = nil;
  base::ScopedObservation<web_distiller::DistilledPagePrefs,
                          web_distiller::DistilledPagePrefs::Observer>
      observation_{this};
};

#endif  // COMPONENTS_WEB_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_
