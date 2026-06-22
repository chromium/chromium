// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

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
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.ui.actions.DelegatingActionView;
import org.chromium.chrome.browser.ui.actions.TintedActionView;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

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
    private @Nullable @BrandedColorScheme Integer mColorScheme;

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

    /**
     * Sets a custom layout resource for the ViewStub before it is inflated.
     *
     * @param layoutResource The layout resource to inflate.
     */
    /*package*/ void setStubLayoutResource(int layoutResource) {
        assert mTargetView == null : "Stub already inflated.";
        View child = getChildAt(0);
        if (child instanceof ViewStub stub) {
            stub.setLayoutResource(layoutResource);
        }
    }

    /** Inflates the child ViewStub. */
    /*package*/ void inflateStub() {
        View child = getChildAt(0);
        if (child instanceof ViewStub stub) {
            mTargetView = stub.inflate();
            ColorStateList oldTint = null;
            if (mTargetView instanceof ImageView imageView) {
                oldTint = imageView.getImageTintList();
            }
            applyColorScheme(mIconTint, oldTint);
            applyTargetBackground();
        }
        assert mTargetView != null : "Stub inflation failed.";
    }

    /**
     * Sets the color scheme and tint for the target view.
     *
     * @param tint The color state list to apply.
     * @param colorScheme The {@link BrandedColorScheme} to apply.
     */
    /*package*/ void setColorScheme(ColorStateList tint, @BrandedColorScheme int colorScheme) {
        ColorStateList oldTint = mIconTint;
        mIconTint = tint;
        mColorScheme = colorScheme;
        applyColorScheme(mIconTint, oldTint);
    }

    private void applyColorScheme(
            @Nullable ColorStateList newTint, @Nullable ColorStateList oldTint) {
        if (mTargetView instanceof TintObserver observer && mColorScheme != null) {
            observer.onTintChanged(newTint, null, mColorScheme);
        } else if (mTargetView instanceof ImageView imageView) {
            // Only apply the new themed tint if the ImageView is currently using the old
            // themed tint. If the ImageView has a custom tint list (an active override),
            // we preserve it to prevent clobbering.
            if (newTint != null && Objects.equals(imageView.getImageTintList(), oldTint)) {
                imageView.setImageTintList(newTint);
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
        applyTargetBackground();
    }

    private void applyTargetBackground() {
        if (mTargetView == null || mTargetBackground == null) return;
        mTargetView.setBackground(mTargetBackground);
    }
}
