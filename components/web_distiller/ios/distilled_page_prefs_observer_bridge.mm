// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/web_distiller/ios/distilled_page_prefs_observer_bridge.h"

DistilledPagePrefsObserverBridge::DistilledPagePrefsObserverBridge(
    id<DistilledPagePrefsObserving> observer,
    web_distiller::DistilledPagePrefs* distilled_page_prefs)
    : observer_(observer) {
  observation_.Observe(distilled_page_prefs);
}

DistilledPagePrefsObserverBridge::~DistilledPagePrefsObserverBridge() = default;

void DistilledPagePrefsObserverBridge::OnChangeFontFamily(
    web_distiller::mojom::FontFamily font) {
  [observer_ onChangeFontFamily:font];
}

void DistilledPagePrefsObserverBridge::OnChangeTheme(
    web_distiller::mojom::Theme theme,
    web_distiller::ThemeSettingsUpdateSource source) {
  [observer_ onChangeTheme:theme withSource:source];
}

void DistilledPagePrefsObserverBridge::OnChangeFontScaling(float scaling) {
  [observer_ onChangeFontScaling:scaling];
}

void DistilledPagePrefsObserverBridge::OnChangeLinksEnabled(bool enabled) {
  [observer_ onChangeLinksEnabled:enabled];
}
