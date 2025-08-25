// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** The properties associated with the Navigation Attachments bar. */
@NullMarked
class NavigationAttachmentsProperties {
    /** The adapter for the attachments RecyclerView. */
    public static final WritableObjectPropertyKey<SimpleRecyclerViewAdapter> ADAPTER =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Add button. */
    public static final WritableObjectPropertyKey<Runnable> BUTTON_ADD_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Camera button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_CAMERA_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Gallery button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_GALLERY_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the navigation toolbar is visible. */
    public static final WritableBooleanPropertyKey TOOLBAR_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        ADAPTER, BUTTON_ADD_CLICKED, POPUP_CAMERA_CLICKED, POPUP_GALLERY_CLICKED, TOOLBAR_VISIBLE
    };
}
