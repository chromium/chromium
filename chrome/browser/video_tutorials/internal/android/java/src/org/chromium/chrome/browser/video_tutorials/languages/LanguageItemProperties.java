// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with the language list items.
 */
class LanguageItemProperties {
    /** The view type used by the recycler view to show the language list item. */
    public static final int ITEM_VIEW_TYPE = 1;

    /** The associated locale.*/
    static final WritableObjectPropertyKey<String> LOCALE = new WritableObjectPropertyKey<>();

    /** The language name. Shown in the system text. */
    static final WritableObjectPropertyKey<String> NAME = new WritableObjectPropertyKey<>();

    /** The language name in its native text.*/
    static final WritableObjectPropertyKey<String> NATIVE_NAME = new WritableObjectPropertyKey<>();

    /** Whether this language is currently selected.*/
    static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    /** The callback to be invoked on selecting this language.*/
    static final WritableObjectPropertyKey<Callback<String>> SELECTION_CALLBACK =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {LOCALE, NAME, NATIVE_NAME, IS_SELECTED, SELECTION_CALLBACK};
}
