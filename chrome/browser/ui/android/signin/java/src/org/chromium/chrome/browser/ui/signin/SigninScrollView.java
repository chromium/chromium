// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewTreeObserver;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

/**
 * ScrollView without the top edge that also sends notification when it is scrolled to the bottom.
 */
public class SigninScrollView extends ScrollView {
    private final ViewTreeObserver.OnGlobalLayoutListener mOnGlobalLayoutListener =
            this::checkScrolledToBottom;
    private final ViewTreeObserver.OnScrollChangedListener mOnScrollChangedListener =
            this::checkScrolledToBottom;
    private @Nullable Runnable mObserver;

    public SigninScrollView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected float getTopFadingEdgeStrength() {
        // Disable fading out effect at the top of this ScrollView.
        return 0;
    }

    @Override
    protected void onDetachedFromWindow() {
        removeObserver();
        super.onDetachedFromWindow();
    }

    private void checkScrolledToBottom() {
        if (mObserver == null) return;
        if (getChildCount() == 0) {
            // The ScrollView is definitely scrolled to bottom if there are no children.
            mObserver.run();
            return;
        }
        if ((getHeight() + getScrollY()) < getChildAt(getChildCount() - 1).getBottom()) return;
        mObserver.run();
    }

    /**
     * Sets observer. Regardless of the passed value, notifications for the previous observer will
     * be canceled.
     * @param observer The Runnable to receive notification when SigninScrollView is scrolled to
     *         bottom, or null to clear the observer.
     */
    public void setScrolledToBottomObserver(@Nullable Runnable observer) {
        removeObserver();
        if (observer == null) return;
        mObserver = observer;
        getViewTreeObserver().addOnGlobalLayoutListener(mOnGlobalLayoutListener);
        getViewTreeObserver().addOnScrollChangedListener(mOnScrollChangedListener);
    }

    private void removeObserver() {
        if (mObserver == null) return;
        mObserver = null;
        getViewTreeObserver().removeOnGlobalLayoutListener(mOnGlobalLayoutListener);
        getViewTreeObserver().removeOnScrollChangedListener(mOnScrollChangedListener);
    }
}
