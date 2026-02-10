// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static android.view.View.MeasureSpec.EXACTLY;
import static android.view.View.MeasureSpec.UNSPECIFIED;
import static android.view.View.MeasureSpec.makeMeasureSpec;

import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.Px;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the omnibox chip. */
@NullMarked
class OmniboxChipCoordinator {
    private final OmniboxChipMediator mMediator;
    private final MaterialButton mView;

    /**
     * Constructs a coordinator for the given omnibox chip view.
     *
     * @param root The root {@link ViewGroup} that will hold the chip.
     * @param model The {@link PropertyModel} for the chip.
     */
    OmniboxChipCoordinator(ViewGroup root, PropertyModel model) {
        mView =
                (MaterialButton)
                        LayoutInflater.from(root.getContext())
                                .inflate(R.layout.omnibox_chip_full, null);
        root.addView(mView);
        PropertyModelChangeProcessor.create(model, mView, OmniboxChipViewBinder::bind);
        mMediator = new OmniboxChipMediator(model);
    }

    void destroy() {
        var parent = (ViewGroup) mView.getParent();
        if (parent != null) {
            parent.removeView(mView);
        }
    }

    /**
     * Updates the chip's properties.
     *
     * @param text The text to display when the chip is in its expanded state.
     * @param icon The icon drawable to display on the chip.
     * @param contentDesc The content description of the chip.
     * @param onClick A runnable to execute when the chip is clicked.
     */
    void updateChip(String text, Drawable icon, String contentDesc, Runnable onClick) {
        mMediator.updateChip(text, icon, contentDesc, onClick);
    }

    void setAvailableWidth(@Px int availableWidth) {
        mMediator.setAvailableWidth(availableWidth);
    }

    @Px
    int measureWidth() {
        // TODO(crbug.com/450253146): Make sure text isn't measured multiple times unnecessarily.
        mView.measure(
                makeMeasureSpec(0, UNSPECIFIED),
                makeMeasureSpec(mView.getMeasuredHeight(), EXACTLY));
        return mView.getMeasuredWidth();
    }
}
