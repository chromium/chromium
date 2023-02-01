// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * The properties associated with rendering the pedal suggestion view.
 */
public class PedalSuggestionViewProperties {
    /**
     * ViewType defines a list of Views that are understood by the Carousel.
     * Views below can be used by any instance of the carousel, guaranteeing that each instance
     * will look like every other.
     */
    @IntDef({ViewType.PEDAL_VIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        /** Carousel item is a PedalView instance. */
        public int PEDAL_VIEW = 0;
    }

    /** Omnibox Pedal list descriptions. */
    public static final WritableObjectPropertyKey<List<ListItem>> PEDAL_LIST =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {PEDAL_LIST};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionViewProperties.ALL_KEYS);
}