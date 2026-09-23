// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility methods for the Bottom Sheet. */
@NullMarked
public final class BottomSheetUtils {
    private BottomSheetUtils() {}

    /**
     * Returns whether the bottom sheet controller holds content that acts as browser controls,
     * checking the feature flag and whether it is enabled for the current context as well.
     *
     * @param controller The {@link BottomSheetController} to check.
     * @param isBottomSheetAsBrowserControlsEnabled Whether bottom sheet acting as browser controls
     *     is enabled for the current context.
     */
    public static boolean isContentActingAsBrowserControls(
            @Nullable BottomSheetController controller,
            boolean isBottomSheetAsBrowserControlsEnabled) {
        if (controller == null) return false;
        if (!isBottomSheetAsBrowserControlsEnabled) return false;

        BottomSheetContent content = controller.getCurrentSheetContent();
        return content != null && content.actsAsBrowserControls() && controller.isFullWidth();
    }

    /**
     * Returns whether the given {@link BottomSheetContent} is non-modal.
     *
     * @param content The {@link BottomSheetContent} to check.
     * @return True if the content is non-modal, false otherwise.
     */
    public static boolean isSheetNonModal(@Nullable BottomSheetContent content) {
        if (content == null) return false;
        if (BottomSheetFeatureMap.sBottomSheetTypes.isEnabled()) {
            return !content.getSheetType().isModal();
        }
        return content.hasCustomScrimLifecycle();
    }

    /**
     * Returns whether the bottom sheet framework supplies its own close button for the given
     * content. This is the single definition of that rule: sheet contents must consult it rather
     * than re-deriving it, so that content which draws its own close button hides that button
     * exactly when the framework replaces it.
     *
     * <p>Modal sheets are dismissible through their scrim, so the framework deliberately adds no
     * close button of its own for them.
     *
     * @param isLargeFormFactorPopup Whether the sheet is showing as a large form factor popup.
     * @param content The {@link BottomSheetContent} to check.
     */
    public static boolean shouldShowFrameworkCloseButton(
            boolean isLargeFormFactorPopup, @Nullable BottomSheetContent content) {
        return isLargeFormFactorPopup && isSheetNonModal(content);
    }
}
