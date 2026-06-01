// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;

/** Helper class for checking Fusebox-related features. */
@NullMarked
public class FuseboxFeatureUtils {

    private FuseboxFeatureUtils() {}

    /**
     * Returns whether the New Tab Page "plus" button should be displayed.
     *
     * @param context The current context.
     * @param profile The active profile.
     * @param templateUrlService The template URL service.
     */
    public static boolean shouldShowNtpPlusButton(
            Context context,
            @Nullable Profile profile,
            @Nullable TemplateUrlService templateUrlService) {
        return profile != null
                && !profile.isOffTheRecord()
                && OmniboxFeatures.isMultimodalInputEnabled(context)
                && OmniboxFeatures.sShowNtpPlusButton.getValue()
                && OmniboxCapabilities.isFuseboxSupportedDeviceType()
                && ComposeboxQueryControllerBridge.isFuseboxEligibleForProfile(profile)
                && templateUrlService != null
                && templateUrlService.isDefaultSearchEngineGoogle();
    }
}
