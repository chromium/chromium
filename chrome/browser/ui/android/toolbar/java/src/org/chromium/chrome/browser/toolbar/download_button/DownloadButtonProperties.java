// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.content.res.ColorStateList;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the download toolbar button model. */
@NullMarked
final class DownloadButtonProperties {
    /** Indicates whether the download button should be displayed based on state. */
    public static final WritableBooleanPropertyKey SHOULD_SHOW =
            new WritableBooleanPropertyKey("should_show");

    /**
     * Indicates whether the download button is visible. This is true only if SHOULD_SHOW is true
     * and there is space in the toolbar.
     */
    public static final WritableBooleanPropertyKey IS_VISIBLE =
            new WritableBooleanPropertyKey("is_visible");

    /** Click listener for the download button. */
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>("on_click");

    /** The tint color applied to the download button icon. */
    public static final WritableObjectPropertyKey<ColorStateList> TINT =
            new WritableObjectPropertyKey<>("tint");

    /** Whether the toolbar is in incognito mode (used for button ripple). */
    public static final WritableBooleanPropertyKey IS_INCOGNITO =
            new WritableBooleanPropertyKey("is_incognito");

    /** The drawable resource for the button icon. */
    public static final WritableIntPropertyKey ICON_RES = new WritableIntPropertyKey("icon_res");

    /** How the progress ring is drawn, as a {@link DownloadProgressRingDrawable.RingStatus}. */
    public static final WritableIntPropertyKey RING_STATUS =
            new WritableIntPropertyKey("ring_status");

    /** Aggregate download progress in [0, 100], used while the ring status is DOWNLOADING. */
    public static final WritableIntPropertyKey PROGRESS_PERCENT =
            new WritableIntPropertyKey("progress_percent");

    /** Number shown in the badge, or 0 to hide it. */
    public static final WritableIntPropertyKey BADGE_COUNT =
            new WritableIntPropertyKey("badge_count");

    /** The toolbar background color, used to fill the badge so it stands out from the ring. */
    public static final WritableIntPropertyKey BADGE_BACKGROUND_COLOR =
            new WritableIntPropertyKey("badge_background_color");

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SHOULD_SHOW,
                IS_VISIBLE,
                ON_CLICK,
                TINT,
                IS_INCOGNITO,
                ICON_RES,
                RING_STATUS,
                PROGRESS_PERCENT,
                BADGE_COUNT,
                BADGE_BACKGROUND_COLOR
            };
}
