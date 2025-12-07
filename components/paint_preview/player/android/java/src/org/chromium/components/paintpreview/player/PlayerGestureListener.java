// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/**
 * Records metrics and handles player-wide (as opposed to per-frame) logic related to touch
 * gestures.
 */
@NullMarked
public class PlayerGestureListener {
    private final Runnable mUserInteractionCallback;
    private final LinkClickHandler mLinkClickHandler;
    private @Nullable PlayerUserFrustrationDetector mUserFrustrationDetector;

    public PlayerGestureListener(
            LinkClickHandler linkClickHandler,
            Runnable userInteractionCallback,
            Runnable userFrustrationCallback) {
        TraceEvent.begin("PlayerGestureListener");
        mLinkClickHandler = linkClickHandler;
        mUserInteractionCallback = userInteractionCallback;
        if (userFrustrationCallback == null) return;

        mUserFrustrationDetector = new PlayerUserFrustrationDetector(userFrustrationCallback);
        TraceEvent.end("PlayerGestureListener");
    }

    /**
     * Called when a tap gesture happens in the player.
     * @param url The GURL of the tapped link. If there are no links in the tapped region, this will
     *            be null.
     */
    public void onTap(@Nullable GURL url) {
        if (url != null && mLinkClickHandler != null) {
            mLinkClickHandler.onLinkClicked(url);
            PlayerUserActionRecorder.recordLinkClick();
            return;
        }

        if (mUserFrustrationDetector != null) mUserFrustrationDetector.recordUnconsumedTap();
        PlayerUserActionRecorder.recordUnconsumedTap();
    }

    public void onLongPress() {
        if (mUserFrustrationDetector != null) mUserFrustrationDetector.recordUnconsumedLongPress();
        PlayerUserActionRecorder.recordLongPress();
    }

    public void onFling() {
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        PlayerUserActionRecorder.recordFling();
    }

    public void onScroll() {
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        PlayerUserActionRecorder.recordScroll();
    }

    public void onScale(boolean didFinish) {
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        if (didFinish) PlayerUserActionRecorder.recordZoom();
    }
}
