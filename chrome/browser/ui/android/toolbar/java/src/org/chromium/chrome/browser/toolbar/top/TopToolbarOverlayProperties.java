// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties associated with the top toolbar's composited layer. */
public class TopToolbarOverlayProperties {
    /** The ID of the toolbar's texture resource. */
    public static final ReadableIntPropertyKey RESOURCE_ID = new ReadableIntPropertyKey();

    /** The texture resource used to draw the location/URL bar. */
    public static final ReadableIntPropertyKey URL_BAR_RESOURCE_ID = new ReadableIntPropertyKey();

    /** The background color of the toolbar. */
    public static final WritableIntPropertyKey TOOLBAR_BACKGROUND_COLOR =
            new WritableIntPropertyKey();

    /** The color of the location/URL bar. */
    public static final WritableIntPropertyKey URL_BAR_COLOR = new WritableIntPropertyKey();

    /** The current offset of the top toolbar. */
    public static final WritableFloatPropertyKey Y_OFFSET = new WritableFloatPropertyKey();

    /** Whether the shadow under the toolbar should be visible. */
    public static final WritableBooleanPropertyKey SHOW_SHADOW = new WritableBooleanPropertyKey();

    /** Whether the layer should be visible. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** A "struct" for progress bar drawing info. */
    public static final WritableObjectPropertyKey<DrawingInfo> PROGRESS_BAR_INFO =
            new WritableObjectPropertyKey<>(true);

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {RESOURCE_ID, URL_BAR_RESOURCE_ID, TOOLBAR_BACKGROUND_COLOR,
                    URL_BAR_COLOR, Y_OFFSET, SHOW_SHADOW, VISIBLE, PROGRESS_BAR_INFO};
}
