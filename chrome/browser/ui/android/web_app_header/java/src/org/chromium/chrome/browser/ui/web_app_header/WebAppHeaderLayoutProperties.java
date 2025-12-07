// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Properties of the {@link PropertyModel} that reflect the state of webapp header */
@NullMarked
class WebAppHeaderLayoutProperties {

    /** The paddings rect that indicates how much to offset children. */
    static final WritableObjectPropertyKey<Rect> PADDINGS = new WritableObjectPropertyKey<>();

    /** Header's minimum height */
    static final WritableIntPropertyKey MIN_HEIGHT = new WritableIntPropertyKey();

    /** The visibility of the webapp header. */
    static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    static final WritableObjectPropertyKey<List<Rect>> NON_DRAGGABLE_AREAS =
            new WritableObjectPropertyKey<>();

    static final WritableObjectPropertyKey<Callback<Integer>> WIDTH_CHANGED_CALLBACK =
            new WritableObjectPropertyKey<>();

    static final WritableObjectPropertyKey<Callback<Integer>> VISIBILITY_CHANGED_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** Background color of the header */
    static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    static final WritableObjectPropertyKey<List<Rect>> BACKGROUND_CUTOUTS =
            new WritableObjectPropertyKey<>();

    /** The set of all model properties. */
    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PADDINGS,
                IS_VISIBLE,
                MIN_HEIGHT,
                NON_DRAGGABLE_AREAS,
                WIDTH_CHANGED_CALLBACK,
                BACKGROUND_COLOR,
                BACKGROUND_CUTOUTS,
                VISIBILITY_CHANGED_CALLBACK,
            };

    private WebAppHeaderLayoutProperties() {}
}
