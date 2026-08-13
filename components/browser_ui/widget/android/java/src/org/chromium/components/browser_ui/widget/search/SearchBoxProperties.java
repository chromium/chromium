// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.search;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the generic desktop search box. */
@NullMarked
public class SearchBoxProperties {
    public static final WritableObjectPropertyKey<Callback<String>> TEXT_CHANGED_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SEARCH_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> HINT_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<Boolean>> FOCUS_CHANGED_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey HAS_FOCUS = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Runnable> CLEAR_SEARCH_TEXT_RUNNABLE =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey CLEAR_BUTTON_VISIBILITY =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey SEARCH_LOUPE_VISIBILITY =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TEXT_CHANGED_CALLBACK,
        SEARCH_TEXT,
        HINT_TEXT,
        FOCUS_CHANGED_CALLBACK,
        HAS_FOCUS,
        CLEAR_SEARCH_TEXT_RUNNABLE,
        CLEAR_BUTTON_VISIBILITY,
        SEARCH_LOUPE_VISIBILITY
    };
}
