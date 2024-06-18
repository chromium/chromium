// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The public interface for the bottom sheet's controller. Features wishing to show content in the
 * sheet UI must implement {@link BottomSheetContent} and call
 * {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the content
 * was actually shown (see full doc on method).
 */
public interface BottomSheetController {
    /** The different states that the bottom sheet can have. */
    @IntDef({
        SheetState.NONE,
        SheetState.HIDDEN,
        SheetState.PEEK,
        SheetState.HALF,
        SheetState.FULL,
        SheetState.SCROLLING
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SheetState {
        /**
         * NONE is for internal use only and indicates the sheet is not currently
         * transitioning between states.
         */
        int NONE = -1;

        // Values are used for indexing mStateRatios, should start from 0
        // and can't have gaps. Additionally order is important for these,
        // they go from smallest to largest.
        int HIDDEN = 0;
        int PEEK = 1;
        int HALF = 2;
        int FULL = 3;

        int SCROLLING = 4;
    }

    /**
     * The different reasons that the sheet's state can change.
     *
     * Needs to stay in sync with BottomSheet.StateChangeReason in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        StateChangeReason.NONE,
        StateChangeReason.SWIPE,
        StateChangeReason.BACK_PRESS,
        StateChangeReason.TAP_SCRIM,
        StateChangeReason.NAVIGATION,
        StateChangeReason.COMPOSITED_UI,
        StateChangeReason.VR,
        StateChangeReason.PROMOTE_TAB,
        StateChangeReason.OMNIBOX_FOCUS,
        StateChangeReason.INTERACTION_COMPLETE,
        StateChangeReason.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StateChangeReason {
        int NONE = 0;
        int SWIPE = 1;
        int BACK_PRESS = 2;
        int TAP_SCRIM = 3;
        int NAVIGATION = 4;
        int COMPOSITED_UI = 5;
        int VR = 6;
        int PROMOTE_TAB = 7;
        int OMNIBOX_FOCUS = 8;
        int INTERACTION_COMPLETE = 9;

        // STOP: Updates here require an update in enums.xml.
        int MAX_VALUE = INTERACTION_COMPLETE;
    }

    /**
     * Request that some content be shown in the bottom sheet.
     * @param content The content to be shown in the bottom sheet.
     * @param animate Whether the appearance of the bottom sheet should be animated.
     * @return True if the content was shown, false if it was suppressed. Content is suppressed if
     *         higher priority content is in the sheet, the sheet is expanded beyond the peeking
     *         state, or the browser is in a mode that does not support showing the sheet.
     */
    boolean requestShowContent(BottomSheetContent content, boolean animate);

    /**
     * Hide content shown in the bottom sheet. If the content is not showing, this call retracts the
     * request to show it.
     * @param content The content to be hidden.
     * @param animate Whether the sheet should animate when hiding.
     * @param hideReason The reason that the content is being hidden.
     */
    void hideContent(
            BottomSheetContent content, boolean animate, @StateChangeReason int hideReason);

    void hideContent(BottomSheetContent content, boolean animate);

    /** @param observer The observer to add. */
    void addObserver(BottomSheetObserver observer);

    /** @param observer The observer to remove. */
    void removeObserver(BottomSheetObserver observer);

    /** Expand the sheet. If there is no content in the sheet, this is a noop. */
    void expandSheet();

    /**
     * Collapse the current sheet to peek state. Sheet may not change the state if the state
     * is not allowed.
     * @param animate {@code true} for animation effect.
     * @return {@code true} if the sheet could go to the peek state.
     */
    boolean collapseSheet(boolean animate);

    /** @return The content currently showing in the bottom sheet. */
    BottomSheetContent getCurrentSheetContent();

    /** @return The current state of the bottom sheet. */
    @SheetState
    int getSheetState();

    /** @return The target state of the bottom sheet (usually during animations). */
    @SheetState
    int getTargetSheetState();

    /** @return Whether the bottom sheet is currently open (expanded beyond peek state). */
    boolean isSheetOpen();

    /** @return Whether the bottom sheet is in the process of hiding. */
    boolean isSheetHiding();

    /** @return The current offset from the bottom of the screen that the sheet is in px. */
    int getCurrentOffset();

    /**
     * @return The height of the bottom sheet's container in px. This will return 0 if the sheet has
     *         not been initialized (content has not been requested).
     */
    int getContainerHeight();

    /**
     * @return The srcim's coordinator. This can be used to customize the bottom sheet's interaction
     *         with the scrim if the default behavior is not desired -- fading in behind the sheet
     *         as the sheet is expanded.
     */
    ScrimCoordinator getScrimCoordinator();

    /**
     * This method provides a property model that can be used to show the scrim behind the bottom
     * sheet. This can be used in conjunction with {@link #getScrimCoordinator()} to customize the
     * scrim's behavior. While this method is not required to show the scrim, this method returns
     * a model set up to appear behnind the sheet. Common usage is the following:
     *
     * PropertyModel params = controller.createScrimParams();
     * // further modify params
     * controller.getScrimCoordinator().showScrim(params);
     *
     * @return A property model used to show the scrim behind the bottom sheet.
     */
    PropertyModel createScrimParams();

    /**
     * @return The {@link BackPressHandler} that will handle a back press event when the bottom
     *     sheet is open or holds sheet content.
     */
    BackPressHandler getBottomSheetBackPressHandler();

    /**
     * @return Whether the sheet covers the full width of the container, or is limited to only
     *     partial width.
     */
    boolean isFullWidth();

    /**
     * @return Whether the bottom sheet is being shown on a small screen. This disables the half
     *     sheet state.
     */
    boolean isSmallScreen();
}
