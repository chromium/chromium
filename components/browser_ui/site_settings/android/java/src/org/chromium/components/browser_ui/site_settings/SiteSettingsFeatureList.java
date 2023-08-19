// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

/**
 * Lists base::Features that can be accessed through {@link SiteSettingsFeatureMap}.
 *
 * Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/browser_ui/site_settings/android/site_settings_feature_map.cc
 */
public abstract class SiteSettingsFeatureList {
    public static final String SITE_DATA_IMPROVEMENTS = "SiteDataImprovements";
}
