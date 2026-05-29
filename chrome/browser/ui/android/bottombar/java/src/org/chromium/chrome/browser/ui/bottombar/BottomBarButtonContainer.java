// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.DelegatingActionView;
import org.chromium.chrome.browser.ui.actions.TintedActionView;

import java.util.Objects;

/**
 * A container for bottom bar buttons that delegates action properties to its child view. This
 * container resolves layout gaps by handling its own visibility.
 */
@NullMarked
public class BottomBarButtonContainer extends FrameLayout
        implements DelegatingActionView, TintedActionView {

    private @Nullable ColorStateList mIconTint;
    private @Nullable View mTargetView;
    private @Nullable Drawable mTargetBackground;

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
        if (mTargetView == null) {
            inflateStub();
        }
        assert mTargetView != null;
        return mTargetView;
    }

    /*package*/ void inflateStub(@ActionId int actionId) {
        inflateStub();
        if (actionId == ActionId.APP_MENU) {
            assumeNonNull(mTargetView).setTag(R.id.is_bottom_bar_menu_anchor, true);
        }
    }

    /** Inflates the child ViewStub. */
    /*package*/ void inflateStub() {
        View child = getChildAt(0);
        if (child instanceof ViewStub stub) {
            mTargetView = stub.inflate();
            if (mTargetView instanceof ImageView imageView && mIconTint != null) {
                imageView.setImageTintList(mIconTint);
            }
            if (mTargetBackground != null) {
                mTargetView.setBackground(mTargetBackground);
            }
        }
        assert mTargetView != null : "Stub inflation failed.";
    }

    /**
     * Sets the tint for the icon in the target view, if it is an ImageView.
     *
     * @param tint The color state list to apply.
     */
    /*package*/ void setIconTint(ColorStateList tint) {
        ColorStateList oldTint = mIconTint;
        mIconTint = tint;
        if (mTargetView instanceof ImageView imageView) {
            // Only apply the new themed tint if the ImageView is currently using the old
            // themed tint. If the ImageView has a custom tint list (an active override),
            // we preserve it to prevent clobbering.
            if (Objects.equals(imageView.getImageTintList(), oldTint)) {
                imageView.setImageTintList(tint);
            }
        }
    }

    @Override
    public @Nullable ColorStateList getIconTint() {
        return mIconTint;
    }

    /** Returns whether the target view is set/inflated. */
    /*package*/ boolean hasTargetView() {
        return mTargetView != null;
    }

    /**
     * Sets the background drawable of the target view.
     *
     * @param drawable The drawable to set as background.
     */
    /*package*/ void setTargetBackground(Drawable drawable) {
        mTargetBackground = drawable;
        if (mTargetView != null) {
            mTargetView.setBackground(drawable);
        }
    }
}
