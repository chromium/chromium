// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;

import java.util.Collection;

/** Manager that manages showing and hiding omnibox chips. */
@NullMarked
public class OmniboxChipManager implements ToolbarWidthConsumer {
    private final ViewGroup mRootView;

    // TODO(crbug.com/450253146): Move to its MVC component.
    private @Nullable MaterialButton mOpenInAppChip;

    /**
     * Creates an instance of {@link OmniboxChipManager}.
     *
     * @param rootView The root {@link ViewGroup} that will house the chip views.
     */
    public OmniboxChipManager(ViewGroup rootView) {
        mRootView = rootView;
    }

    public void destroy() {
        if (mOpenInAppChip != null) {
            mOpenInAppChip.setOnClickListener(null);
        }
    }

    public void showChip(@Nullable String text, @Nullable Drawable icon, Runnable onClick) {
        dismissChip();

        mOpenInAppChip =
                (MaterialButton)
                        LayoutInflater.from(mRootView.getContext())
                                .inflate(R.layout.omnibox_chip_full, null);
        mRootView.addView(mOpenInAppChip);
        mRootView.setVisibility(View.VISIBLE);
        mOpenInAppChip.setText(text);
        mOpenInAppChip.setIcon(icon);
        mOpenInAppChip.setOnClickListener(view -> onClick.run());
    }

    public void dismissChip() {
        if (mOpenInAppChip != null) {
            mRootView.removeView(mOpenInAppChip);
            mRootView.setVisibility(View.GONE);
            mOpenInAppChip = null;
        }
    }

    // TODO(crbug.com/450253146): Integrate with the scalable toolbar/ToolbarWidthConsumer system.
    @Override
    @EnsuresNonNullIf("mOpenInAppChip")
    public boolean isVisible() {
        return mOpenInAppChip != null;
    }

    @Override
    public int updateVisibility(int availableWidth) {
        return isVisible() ? mOpenInAppChip.getMeasuredWidth() : 0;
    }

    @Override
    public int updateVisibilityWithAnimation(int availableWidth, Collection<Animator> animators) {
        return isVisible() ? mOpenInAppChip.getMeasuredWidth() : 0;
    }
}
