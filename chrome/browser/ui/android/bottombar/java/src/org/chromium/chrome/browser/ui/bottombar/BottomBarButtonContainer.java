// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.actions.DelegatingActionView;

/**
 * A container for bottom bar buttons that delegates action properties to its child view. This
 * container resolves layout gaps by handling its own visibility.
 */
@NullMarked
public class BottomBarButtonContainer extends FrameLayout implements DelegatingActionView {

    private @Nullable View mTargetView;

    public BottomBarButtonContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        assert getChildCount() == 1 : "BottomBarButtonContainer should only have one child.";
        View child = getChildAt(0);
        if (!(child instanceof ViewStub)) {
            mTargetView = child;
        }
    }

    @Override
    public View getTargetView() {
        assert mTargetView != null : "Target view wasn't set.";
        return mTargetView;
    }

    /** Inflates the child ViewStub. */
    public void inflateStub() {
        View child = getChildAt(0);
        if (child instanceof ViewStub stub) {
            mTargetView = stub.inflate();
        }
        assert mTargetView != null : "Stub inflation failed.";
    }

    /**
     * Sets the tint for the icon in the target view, if it is an ImageView.
     *
     * @param tint The color state list to apply.
     */
    public void setIconTint(ColorStateList tint) {
        if (mTargetView instanceof ImageView imageView) {
            imageView.setImageTintList(tint);
        }
    }
}
