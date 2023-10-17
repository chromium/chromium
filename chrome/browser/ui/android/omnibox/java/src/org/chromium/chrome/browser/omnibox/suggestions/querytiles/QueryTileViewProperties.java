// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with the QueryTileView. */
/* package */ @interface QueryTileViewProperties {
    /** The background to apply to the QueryTile. */
    static final WritableObjectPropertyKey<Drawable> IMAGE = new WritableObjectPropertyKey<>();

    /** The title of the QueryTile. */
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {IMAGE, TITLE};
}
