// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties needed to show the video tutorial IPH. */
class VideoIPHProperties {
    /** Whether or not the IPH should be shown. */
    public static final WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();

    /** The display title. */
    public static final WritableObjectPropertyKey<String> DISPLAY_TITLE =
            new WritableObjectPropertyKey<>();

    /** The text representing the length of the video. */
    public static final WritableObjectPropertyKey<String> VIDEO_LENGTH =
            new WritableObjectPropertyKey<>();

    /** The thumbnail associated with the tutorial. */
    public static final WritableObjectPropertyKey<Bitmap> THUMBNAIL =
            new WritableObjectPropertyKey<>();

    /** The listener to be invoked when the IPH is clicked. */
    public static final WritableObjectPropertyKey<Runnable> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The listener to be invoked when the IPH is dismissed. */
    public static final WritableObjectPropertyKey<Runnable> DISMISS_LISTENER =
            new WritableObjectPropertyKey<>();

    /** All keys associated with the model. */
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            VISIBILITY, DISPLAY_TITLE, VIDEO_LENGTH, THUMBNAIL, CLICK_LISTENER, DISMISS_LISTENER};
}
