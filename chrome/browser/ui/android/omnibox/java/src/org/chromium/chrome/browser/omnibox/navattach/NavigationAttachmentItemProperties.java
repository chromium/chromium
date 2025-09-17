// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with a single navigation attachment item. */
@NullMarked
class NavigationAttachmentItemProperties {
    /** The description of the attachment. */
    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The handler for a remove button click. */
    public static final WritableObjectPropertyKey<Runnable> ON_REMOVE =
            new WritableObjectPropertyKey<>();

    /** The thumbnail of the attachment. */
    public static final WritableObjectPropertyKey<Drawable> THUMBNAIL =
            new WritableObjectPropertyKey<>();

    /** The name of the attachment. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {DESCRIPTION, ON_REMOVE, THUMBNAIL, TITLE};
}
