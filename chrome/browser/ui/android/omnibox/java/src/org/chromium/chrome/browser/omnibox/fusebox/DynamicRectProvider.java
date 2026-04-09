// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.res.Resources;
import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupState;
import org.chromium.ui.widget.RectProvider;

/** A {@link RectProvider} that can switch between different delegates. */
@NullMarked
class DynamicRectProvider extends RectProvider implements RectProvider.Observer {
    private final RectProvider mFloatingDelegate;
    private final RectProvider mBottomDelegate;
    private @Nullable RectProvider mCurrentDelegate;
    private boolean mIsObserved;

    /**
     * @param floatingDelegate The {@link RectProvider} for the floating popup.
     * @param bottomDelegate The {@link RectProvider} for the bottom sheet popup.
     */
    public DynamicRectProvider(RectProvider floatingDelegate, RectProvider bottomDelegate) {
        mFloatingDelegate = floatingDelegate;
        mBottomDelegate = bottomDelegate;
    }

    /**
     * Switch the current delegate based on the provided PopupState.
     *
     * @param state The target state of the popup.
     */
    public void setPopupState(@PopupState int state) {
        RectProvider nextDelegate =
                switch (state) {
                    case PopupState.FLOATING -> mFloatingDelegate;
                    case PopupState.BOTTOM -> mBottomDelegate;
                    default -> null;
                };

        if (nextDelegate == mCurrentDelegate) return;

        if (mCurrentDelegate != null) {
            mCurrentDelegate.stopObserving();
        }
        mCurrentDelegate = nextDelegate;
        if (mCurrentDelegate != null) {
            if (mIsObserved) {
                mCurrentDelegate.startObserving(this);
            }
            // Force an immediate UI update.
            onRectChanged();
        } else {
            onRectHidden();
        }
    }

    /** Returns the current RectProvider delegate. */
    public @Nullable RectProvider getCurrentDelegate() {
        return mCurrentDelegate;
    }

    /**
     * Get the desired width for the popup based on the provided state.
     *
     * @param state The target state of the popup.
     * @param resources The resources to use for dimension lookups.
     * @return The desired width in pixels.
     */
    public int getPopupWidth(@PopupState int state, Resources resources) {
        return switch (state) {
            case PopupState.FLOATING ->
                    resources.getDimensionPixelSize(R.dimen.fusebox_popup_width);
            case PopupState.BOTTOM -> mBottomDelegate.getRect().width();
            default -> 0;
        };
    }

    // RectProvider implementation.

    @Override
    public void startObserving(Observer observer) {
        super.startObserving(observer);
        if (mIsObserved) return;
        mIsObserved = true;
        if (mCurrentDelegate != null) {
            mCurrentDelegate.startObserving(this);
        }
    }

    @Override
    public void stopObserving() {
        if (!mIsObserved) return;
        if (mCurrentDelegate != null) {
            mCurrentDelegate.stopObserving();
        }
        mIsObserved = false;
        super.stopObserving();
    }

    @Override
    public Rect getRect() {
        // Fallback to an empty rect rather than super.getRect() to prevent layout churn when
        // hidden.
        return mCurrentDelegate != null ? mCurrentDelegate.getRect() : new Rect();
    }

    // RectProvider.Observer implementation.

    @Override
    public void onRectChanged() {
        notifyRectChanged();
    }

    @Override
    public void onRectHidden() {
        notifyRectHidden();
    }
}
