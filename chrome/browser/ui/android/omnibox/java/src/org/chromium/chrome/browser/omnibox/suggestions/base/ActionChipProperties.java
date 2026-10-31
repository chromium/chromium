// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties of a single Omnibox action chip. Extends {@link ChipProperties} with the Omnibox
 * resource provider so the chip can be themed for the current color scheme at bind time.
 *
 * <p>Chips are presented as a {@code ModelList} of {@link ViewType} items stored on the containing
 * suggestion as {@link BaseSuggestionViewProperties#ACTION_CHIPS}.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public @interface ActionChipProperties {
    /** Item types understood by the action chips carousel. */
    @IntDef({ViewType.CHIP})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @interface ViewType {
        /** Carousel item is a ChipView bound with {@link ActionChipProperties}. */
        int CHIP = 0;
    }

    /** Provider of scheme-dependent Omnibox resources, used to theme the chip. */
    ReadableObjectPropertyKey<OmniboxResourceProvider> RESOURCE_PROVIDER =
            new ReadableObjectPropertyKey<>();

    PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {RESOURCE_PROVIDER};

    PropertyKey[] ALL_KEYS = PropertyModel.concatKeys(ChipProperties.ALL_KEYS, ALL_UNIQUE_KEYS);
}
