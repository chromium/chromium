// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An interface defining content that can be displayed inside of the bottom sheet for Chrome
 * Home.
 */
public interface BottomSheetContent {
    /** The different possible height modes for a given state. */
    @IntDef({HeightMode.DEFAULT, HeightMode.WRAP_CONTENT, HeightMode.DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    @interface HeightMode {
        /**
         * The sheet will use the stock behavior for the {@link BottomSheetController.SheetState}
         * this is used for. Typically this means a pre-defined height ratio, peek being the
         * exception that uses the feature's toolbar height.
         */
        int DEFAULT = 0;

        /**
         * The sheet will set its height so the content is completely visible. This mode cannot
         * be used for the peek state.
         */
        int WRAP_CONTENT = -1;

        /**
         * The state this mode is used for will be disabled. For example, disabling the peek state
         * would cause the sheet to automatically expand when triggered.
         */
        int DISABLED = -2;
    }

    /** The different priorities that the sheet's content can have. */
    @IntDef({ContentPriority.HIGH, ContentPriority.LOW})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContentPriority {
        int HIGH = 0;
        int LOW = 1;
    }

    /** Interface to listen when the size of a BottomSheetContent changes. */
    interface ContentSizeListener {
        /** Called when the size of the view has changed. */
        void onSizeChanged(int width, int height, int oldWidth, int oldHeight);
    }

    /**
     * Gets the {@link View} that holds the content to be displayed in the Chrome Home bottom sheet.
     *
     * @return The content view.
     */
    View getContentView();

    /**
     * Gets the background color for the bottom sheet content, defaulting to the semantic default
     * background color if no background color is specified by the content. This should return null
     * if the sheet content is showing tab content / a page preview.
     */
    @ColorInt
    default Integer getBackgroundColor() {
        return SemanticColorUtils.getDefaultBgColor(getContentView().getContext());
    }

    /**
     * Get the {@link View} that contains the toolbar specific to the content being displayed. If
     * null is returned, the omnibox is used.
     *
     * @return The toolbar view.
     */
    @Nullable
    View getToolbarView();

    /** @return The vertical scroll offset of the content view. */
    int getVerticalScrollOffset();

    /**
     * Called to destroy the {@link BottomSheetContent} when it is dismissed. The means the
     * sheet is in the {@link BottomSheetController.SheetState#HIDDEN} state without being
     * suppressed. This method does not necessarily need to be used but exists for convenience.
     * Cleanup can be done manually via the owning component (likely watching for the sheet hidden
     * event using an observer).
     */
    void destroy();

    /** @return The priority of this content. */
    @ContentPriority
    int getPriority();

    /** @return Whether swiping the sheet down hard enough will cause the sheet to be dismissed. */
    boolean swipeToDismissEnabled();

    /** @return Whether the sheet will always skip the half state once it was fully extended. */
    default boolean skipHalfStateOnScrollingDown() {
        return true;
    }
    ;

    /**
     * @return Whether this content owns its lifecycle. If false, the content will be dismissed
     *         when the user navigates away from the page, switches tabs, or a layout change
     *         occurs.
     *
     * If a BottomSheetContent ever needs to suppress across a layout state change rather than
     * being dismissed this can be addressed by:
     * * Registering a LayoutStateObserver to re-request to be shown when the correct LayoutType
     *   finishes showing.
     * * For LayoutType.BROWSING also registering a TabModelSelectorObserver to dismiss if
     *   a tab switch occurs.
     * * The client is responsible for restoring any previous state of the BottomSheetContent
     *   and the View's it hosts.
     */
    default boolean hasCustomLifecycle() {
        return false;
    }

    /**
     * @return Whether this content owns the scrim lifecycle. If false, a default scrim will
     *         be displayed behind the sheet when this content is shown.
     */
    default boolean hasCustomScrimLifecycle() {
        return false;
    }

    /**
     * @return The height of the peeking state for the content in px or one of the values in
     *         {@link HeightMode}. If {@link HeightMode#DEFAULT}, the system expects
     *         {@link #getToolbarView} to be non-null, where it will then use its height as the
     *         peeking height. This method cannot return {@link HeightMode#WRAP_CONTENT}.
     */
    default int getPeekHeight() {
        return HeightMode.DEFAULT;
    }

    /**
     * @return The height of the half state for the content as a ratio of the height of the
     *         content area (ex. 1.f would be full-screen, 0.5f would be half-screen). The
     *         returned value can also be one of {@link HeightMode}. If
     *         {@link HeightMode#DEFAULT} is returned, the ratio will be a predefined value. If
     *         {@link HeightMode#WRAP_CONTENT} is returned by {@link #getFullHeightRatio()}, the
     *         half height will be disabled. Half height will also be disabled on small screens.
     *         This method cannot return {@link HeightMode#WRAP_CONTENT}.
     */
    default float getHalfHeightRatio() {
        return HeightMode.DEFAULT;
    }

    /**
     * @return The height of the full state for the content as a ratio of the height of the
     *         content area (ex. 1.f would be full-screen, 0.5f would be half-screen). The
     *         returned value can also be one of {@link HeightMode}. If
     *         {@link HeightMode#DEFAULT}, the ratio will be a predefined value. This height
     *         cannot be disabled. This method cannot return {@link HeightMode#DISABLED}.
     */
    default float getFullHeightRatio() {
        return HeightMode.DEFAULT;
    }

    /**
     * @return Whether the sheet should be hidden when it is in the PEEK/HALF state and the user
     *         scrolls down the page.
     */
    default boolean hideOnScroll() {
        return false;
    }

    /**
     * A means for the content to intercept and handle the back press event. This will be called
     * even if the sheet is in the peeking state. If left {@code false}, the sheet will collapse to
     * its minimum state on back press or do nothing if in the minimum / peeking state.
     * @return Whether the bottom sheet handled the back press.
     */
    default boolean handleBackPress() {
        return false;
    }

    /**
     * @return An observable supplier that will hold true if the content will intercept and handle a
     *         back press event, false otherwise. If left {@code false}, the sheet will collapse to
     *         its minimum state on back press or do nothing if in the minimum / peeking state.
     */
    default ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(false);
        return supplier;
    }

    /**
     * Invoked in the event of a back press that is pre-emptively determined by
     * #getBackPressStateChangedSupplier.
     */
    default void onBackPressed() {}

    /**
     * @return The resource id of the content description for the bottom sheet. This is
     *         generally the name of the feature/content that is showing. 'Swipe down to close.'
     *         will be automatically appended after the content description.
     */
    int getSheetContentDescriptionStringId();

    /**
     * @return The resource id of the string announced when the sheet is opened at half height.
     *         This is typically the name of your feature followed by 'opened at half height'.
     */
    int getSheetHalfHeightAccessibilityStringId();

    /**
     * @return The resource id of the string announced when the sheet is opened at full height.
     *         This is typically the name of your feature followed by 'opened at full height'.
     */
    int getSheetFullHeightAccessibilityStringId();

    /**
     * @return The resource id of the string announced when the sheet is closed. This is
     *         typically the name of your feature followed by 'closed'.
     */
    int getSheetClosedAccessibilityStringId();

    /**
     * @return True if this content should hide when higher-priority content is requested to be
     *     shown, even if the sheet is expanded. Otherwise the new content will only be shown after
     *     the sheet is dismissed. If returning true here, this content's priority should be LOW.
     */
    default boolean canSuppressInAnyState() {
        return false;
    }

    /**
     * Whether long press gestures should move the bottom sheet.
     *
     * <p>Should NOT be overridden to return `true` if the bottom sheet contains any UI that
     * responds to long presses. Otherwise bugs will occur when long press is used. See
     * crbug.com/41384419.
     *
     * @return True if long press should move the bottom sheet.
     */
    default boolean shouldLongPressMoveSheet() {
        return false;
    }
}
