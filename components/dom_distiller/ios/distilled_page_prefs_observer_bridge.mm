// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/dom_distiller/ios/distilled_page_prefs_observer_bridge.h"

DistilledPagePrefsObserverBridge::DistilledPagePrefsObserverBridge(
    id<DistilledPagePrefsObserving> observer,
    dom_distiller::DistilledPagePrefs* distilled_page_prefs)
    : observer_(observer) {
  observation_.Observe(distilled_page_prefs);
}

DistilledPagePrefsObserverBridge::~DistilledPagePrefsObserverBridge() = default;

void DistilledPagePrefsObserverBridge::OnChangeFontFamily(
    dom_distiller::mojom::FontFamily font) {
  [observer_ onChangeFontFamily:font];
}

void DistilledPagePrefsObserverBridge::OnChangeTheme(
    dom_distiller::mojom::Theme theme,
    dom_distiller::ThemeSettingsUpdateSource source) {
  [observer_ onChangeTheme:theme withSource:source];
}

void DistilledPagePrefsObserverBridge::OnChangeFontScaling(float scaling) {
  [observer_ onChangeFontScaling:scaling];
}

void DistilledPagePrefsObserverBridge::OnChangeLinksEnabled(bool enabled) {
  [observer_ onChangeLinksEnabled:enabled];
}
