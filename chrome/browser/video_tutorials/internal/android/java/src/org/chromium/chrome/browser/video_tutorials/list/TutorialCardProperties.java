// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties for a video tutorial card.
 */
class TutorialCardProperties {
    /** The view type used by the recycler view to show the tutorial cards. */
    public static final int VIDEO_TUTORIAL_CARD_VIEW_TYPE = 3;

    /** An interface to provide thumbnails for the videos. */
    interface VisualsProvider {
        /** Method to get visuals to display the thumbnail image. */
        void getVisuals(Callback<Drawable> callback);
    }

    /** The title to be shown.*/
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** Text representing the length of the video.*/
    static final WritableObjectPropertyKey<String> VIDEO_LENGTH = new WritableObjectPropertyKey<>();

    /** The callback to invoke when the card is clicked.*/
    static final WritableObjectPropertyKey<Runnable> CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The provider to retrieve the visuals for the video thumbnail.*/
    static final WritableObjectPropertyKey<VisualsProvider> VISUALS_PROVIDER =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TITLE, VIDEO_LENGTH, CLICK_CALLBACK, VISUALS_PROVIDER};
}
