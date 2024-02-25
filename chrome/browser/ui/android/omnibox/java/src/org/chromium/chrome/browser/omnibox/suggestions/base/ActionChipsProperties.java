// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The properties associated with rendering the ActionChipsView. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public @interface ActionChipsProperties {
    /**
     * ViewType defines a list of Views that are understood by the Carousel. Views below can be used
     * by any instance of the carousel, guaranteeing that each instance will look like every other.
     */
    @IntDef({ViewType.CHIP})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public @interface ViewType {
        /** Carousel item is a PedalView instance. */
        public int CHIP = 0;
    }

    /** Action Chip descriptors. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final WritableObjectPropertyKey<ModelList> ACTION_CHIPS =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {ACTION_CHIPS};
}
