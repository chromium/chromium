// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.OffsetTag;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class EdgeToEdgeTopScalpProperties {
    /** The Y offset of the layer in px. */
    static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    /** The height of the top scalp layer in px. */
    static final WritableIntPropertyKey HEIGHT = new WritableIntPropertyKey();

    /** Whether the top scalp component can be shown. */
    static final WritableBooleanPropertyKey CAN_SHOW = new WritableBooleanPropertyKey();

    /** The {@link androidx.annotation.ColorInt} color of the top scalp layer. */
    static final WritableIntPropertyKey COLOR = new WritableIntPropertyKey();

    /** The tag indicating that this layer should be moved by viz. */
    static final WritableObjectPropertyKey<@Nullable OffsetTag> OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    /** Whether the top scalp component is visually visible on screen. */
    static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {Y_OFFSET, HEIGHT, CAN_SHOW, COLOR, OFFSET_TAG, IS_VISIBLE};

    private EdgeToEdgeTopScalpProperties() {}
}
