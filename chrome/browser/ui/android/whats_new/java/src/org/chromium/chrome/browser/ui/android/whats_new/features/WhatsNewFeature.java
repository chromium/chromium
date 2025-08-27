// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new.features;

import android.content.Context;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeatureUtils.WhatsNewType;

/** The interface for an entry in the What's New page. */
@NullMarked
public interface WhatsNewFeature {
    /** Get the {@link WhatsNewType} for this feature. */
    @WhatsNewType
    int getType();

    /** Get a string representing the name of this feature. */
    String getName();

    /** Get the title of the feature to be displayed in the What's New feature list. */
    String getTitle(Context context);

    /** Get the short description of the feature to be displayed in the What's New feature list. */
    String getDescription(Context context);

    /** Get the icon resource id of the feature to be displayed in the What's New feature list. */
    @DrawableRes
    int getIconResId();
}
