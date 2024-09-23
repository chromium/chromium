// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import org.chromium.cc.input.OffsetTag;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties associated with the top toolbar's composited layer. */
public class TopToolbarOverlayProperties {
    /** Whether the URL should be hidden when rendered. */
    public static final WritableBooleanPropertyKey ANONYMIZE = new WritableBooleanPropertyKey();

    /** A "struct" for progress bar drawing info. */
    public static final WritableObjectPropertyKey<DrawingInfo> PROGRESS_BAR_INFO =
            new WritableObjectPropertyKey<>(true);

    /** The ID of the toolbar's texture resource. */
    public static final ReadableIntPropertyKey RESOURCE_ID = new ReadableIntPropertyKey();

    /** Whether the shadow under the toolbar should be visible. */
    public static final WritableBooleanPropertyKey SHOW_SHADOW = new WritableBooleanPropertyKey();

    /** The background color of the toolbar. */
    public static final WritableIntPropertyKey TOOLBAR_BACKGROUND_COLOR =
            new WritableIntPropertyKey();

    /** The color of the location/URL bar. */
    public static final WritableIntPropertyKey URL_BAR_COLOR = new WritableIntPropertyKey();

    /** The texture resource used to draw the location/URL bar. */
    public static final ReadableIntPropertyKey URL_BAR_RESOURCE_ID = new ReadableIntPropertyKey();

    /** Whether the layer should be visible. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** The current x offset of the top toolbar. */
    public static final WritableFloatPropertyKey X_OFFSET = new WritableFloatPropertyKey();

    /** The current y offset of the top toolbar. */
    public static final WritableFloatPropertyKey CONTENT_OFFSET = new WritableFloatPropertyKey();

    /** The OffsetTag indicating that this layer should be moved by viz. */
    public static final WritableObjectPropertyKey<OffsetTag> TOOLBAR_OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ANONYMIZE,
                PROGRESS_BAR_INFO,
                RESOURCE_ID,
                SHOW_SHADOW,
                TOOLBAR_BACKGROUND_COLOR,
                URL_BAR_COLOR,
                URL_BAR_RESOURCE_ID,
                VISIBLE,
                X_OFFSET,
                CONTENT_OFFSET,
                TOOLBAR_OFFSET_TAG
            };
}
