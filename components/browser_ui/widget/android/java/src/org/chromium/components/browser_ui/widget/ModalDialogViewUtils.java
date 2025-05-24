// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Set of shared helper functions for UI related to the {@link ModalDialogView}. */
@NullMarked
public final class ModalDialogViewUtils {
    /** Do not allow instantiation for utils class. */
    private ModalDialogViewUtils() {}

    /**
     * Create a custom button bar with the default modal dialog button setup for the
     * ModalDialogProperty CUSTOM_BUTTON_BAR_VIEW.
     *
     * @param context The current context.
     * @param positiveButton The positive button for the button bar. This may or may not be of type
     *     {@link SpinnerButtonWrapper}.
     * @param negativeButton The negative button for the button bar. This may or may not be of type
     *     {@link SpinnerButtonWrapper}.
     */
    public static View createCustomButtonBarView(
            Context context, View positiveButton, View negativeButton) {
        Resources resources = context.getResources();
        int horizontalPadding =
                resources.getDimensionPixelSize(
                        R.dimen.modal_dialog_control_horizontal_padding_filled);
        int verticalPadding =
                resources.getDimensionPixelSize(
                        R.dimen.modal_dialog_control_vertical_padding_filled);
        DualControlLayout buttonRowLayout = new DualControlLayout(context, null);
        buttonRowLayout.setAlignment(DualControlLayout.DualControlLayoutAlignment.END);
        buttonRowLayout.setStackedMargin(
                resources.getDimensionPixelSize(R.dimen.button_bar_stacked_margin));
        buttonRowLayout.setPadding(
                horizontalPadding, verticalPadding, horizontalPadding, verticalPadding);
        buttonRowLayout.addView(positiveButton);
        buttonRowLayout.addView(negativeButton);
        return buttonRowLayout;
    }
}
