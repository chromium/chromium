// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Encapsulates a UrlBar focus change, including the direction focus arrived from. */
@NullMarked
public class UrlBarFocusChangeInfo {
    /**
     * Indicates that the focus change was not triggered by directional spatial traversal (e.g.
     * arrow keys or Tab/Shift-Tab navigation).
     *
     * <p>Used in non-directional focus scenarios such as:
     *
     * <ul>
     *   <li>Completing UrlBar view reparenting between toolbar and NTP/phone containers
     *       (finishReparenting).
     *   <li>Direct touch taps or clicks on the UrlBar.
     *   <li>Programmatic focus changes and clearing focus.
     * </ul>
     */
    public static final int NO_FOCUS_DIRECTION = 0;

    @IntDef({
        NO_FOCUS_DIRECTION,
        View.FOCUS_BACKWARD,
        View.FOCUS_FORWARD,
        View.FOCUS_LEFT,
        View.FOCUS_UP,
        View.FOCUS_RIGHT,
        View.FOCUS_DOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FocusDirection {}

    public final boolean hasFocus;

    /**
     * The focus traversal direction. Represents a {@link View} focus direction (e.g. {@link
     * View#FOCUS_FORWARD}, {@link View#FOCUS_BACKWARD}) for spatial traversal, or {@link
     * #NO_FOCUS_DIRECTION} when focus is non-directional (e.g. reparenting or touch).
     */
    public final @FocusDirection int direction;

    public UrlBarFocusChangeInfo(boolean hasFocus, @FocusDirection int direction) {
        this.hasFocus = hasFocus;
        this.direction = direction;
    }
}
