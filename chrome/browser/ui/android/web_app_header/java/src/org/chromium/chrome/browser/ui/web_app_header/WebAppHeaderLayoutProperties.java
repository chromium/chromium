// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties of the {@link PropertyModel} that reflect the state of webapp header */
class WebAppHeaderLayoutProperties {

    /** The paddings rect that indicates how much to offset children. */
    static final WritableObjectPropertyKey<Rect> PADDINGS = new WritableObjectPropertyKey<>();

    /** Header's minimum height */
    static final WritableIntPropertyKey MIN_HEIGHT = new WritableIntPropertyKey();

    /** The visibility of the webapp header. */
    static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    /** The set of all model properties. */
    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {PADDINGS, IS_VISIBLE, MIN_HEIGHT};

    private WebAppHeaderLayoutProperties() {}
}
