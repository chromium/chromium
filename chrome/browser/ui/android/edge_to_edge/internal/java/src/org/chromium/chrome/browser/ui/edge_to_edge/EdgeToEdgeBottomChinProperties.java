// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.OffsetTag;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class EdgeToEdgeBottomChinProperties {
    /** The Y offset of the layer in px. */
    static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    /** The height of the bottom chin layer in px. */
    static final WritableIntPropertyKey HEIGHT = new WritableIntPropertyKey();

    /** Whether the bottom chin component can be shown. */
    static final WritableBooleanPropertyKey CAN_SHOW = new WritableBooleanPropertyKey();

    /** The color of the bottom chin layer. */
    static final WritableIntPropertyKey COLOR = new WritableIntPropertyKey();

    /** The color of the divider */
    static final WritableIntPropertyKey DIVIDER_COLOR = new WritableIntPropertyKey();

    /** The tag indicating that this layer should be moved by viz. */
    static final WritableObjectPropertyKey<OffsetTag> OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    /** Whether there are constraint added for the bottom chin to disable its scrolling. */
    static final WritableBooleanPropertyKey HAS_CONSTRAINT = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                Y_OFFSET, HEIGHT, CAN_SHOW, COLOR, DIVIDER_COLOR, OFFSET_TAG, HAS_CONSTRAINT
            };
}
