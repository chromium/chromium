// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.view.View;

/**
 * A class providing basic info required for messages to trigger stacking animation.
 * This is essentially a wrapper and proxy of {@link View}, exposing necessary info fulfilling the
 * minimum requirement.
 */
public class MessageDimens {
    static MessageDimens from(MessageContainer container, View currentView) {
        View siblingView = container.getSiblingView(currentView);
        return new MessageDimens(siblingView);
    }

    private final View mView;
    private boolean mIsMeasured;

    private MessageDimens(View view) {
        mView = view;
    }

    int getHeight() {
        if (!mView.isLaidOut()) {
            if (!mIsMeasured) measure();
            return mView.getMeasuredHeight();
        }
        return mView.getHeight();
    }

    int getWidth() {
        if (!mView.isLaidOut()) {
            if (!mIsMeasured) measure();
            return mView.getMeasuredWidth();
        }
        return mView.getWidth();
    }

    int getTitleHeight() {
        assert mView instanceof MessageBannerView;
        if (!mView.isLaidOut()) {
            if (!mIsMeasured) measure();
            return ((MessageBannerView) mView).getTitleMeasuredHeightForAnimation();
        }
        return ((MessageBannerView) mView).getTitleHeightForAnimation();
    }

    int getDescriptionHeight() {
        assert mView instanceof MessageBannerView;
        if (!mView.isLaidOut()) {
            if (!mIsMeasured) measure();
            return ((MessageBannerView) mView).getDescriptionMeasuredHeightForAnimation();
        }
        return ((MessageBannerView) mView).getDescriptionHeightForAnimation();
    }

    int getPrimaryButtonLineCount() {
        assert mView instanceof MessageBannerView;
        if (!mView.isLaidOut()) {
            if (!mIsMeasured) measure();
        }
        return ((MessageBannerView) mView).getPrimaryButtonLineCountForAnimation();
    }

    private void measure() {
        int maxWidth =
                Math.min(
                        mView.getRootView().getWidth(),
                        mView.getResources().getDimensionPixelSize(R.dimen.message_max_width));
        int wSpec = View.MeasureSpec.makeMeasureSpec(maxWidth, View.MeasureSpec.AT_MOST);
        int hSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        mView.measure(wSpec, hSpec);
        mIsMeasured = true;
    }
}
