// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"

// Protocol that corresponds to the DistilledPagePrefs::Observer API.
@protocol DistilledPagePrefsObserving <NSObject>
@optional
- (void)onChangeFontFamily:(dom_distiller::mojom::FontFamily)font;
- (void)onChangeTheme:(dom_distiller::mojom::Theme)theme
           withSource:(dom_distiller::ThemeSettingsUpdateSource)source;
- (void)onChangeFontScaling:(float)scaling;
- (void)onChangeLinksEnabled:(BOOL)enabled;
@end

// Observer that bridges DistilledPagePrefs events to an Objective-C observer.
class DistilledPagePrefsObserverBridge
    : public dom_distiller::DistilledPagePrefs::Observer {
 public:
  DistilledPagePrefsObserverBridge(
      id<DistilledPagePrefsObserving> observer,
      dom_distiller::DistilledPagePrefs* distilled_page_prefs);
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
  void OnChangeLinksEnabled(bool enabled) override;

 private:
  __weak id<DistilledPagePrefsObserving> observer_ = nil;
  base::ScopedObservation<dom_distiller::DistilledPagePrefs,
                          dom_distiller::DistilledPagePrefs::Observer>
      observation_{this};
};

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLED_PAGE_PREFS_OBSERVER_BRIDGE_H_
