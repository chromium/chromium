// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Embedder-specific behavior of Adaptive Toolbar. */
@NullMarked
public interface AdaptiveToolbarBehavior {

    /** List of the button variants both BrApp/CCT have in common. */
    Set<Integer> COMMON_BUTTONS =
            Set.of(
                    AdaptiveToolbarButtonVariant.SHARE,
                    AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                    AdaptiveToolbarButtonVariant.TRANSLATE,
                    AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                    AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                    AdaptiveToolbarButtonVariant.READER_MODE,
                    AdaptiveToolbarButtonVariant.READ_ALOUD,
                    AdaptiveToolbarButtonVariant.PAGE_SUMMARY);

    /** Default list of valid button variants used for BrApp. */
    Set<Integer> sValidButtons = new HashSet<>();

    /** Returns {@code true} if adaptive toolbar button feature is enabled. */
    default boolean shouldInitialize() {
        return true;
    }

    /** Returns {@code true} if a long click on the button shows a popup menu for settings UI. */
    default boolean canShowSettings() {
        return true;
    }

    /**
     * Returns {@code true} if the button with a chip string should show a text bubble instead of
     * expansion/collapse animation.
     */
    default boolean shouldShowTextBubble() {
        return false;
    }

    /**
     * Register embedder-specific toolbar action buttons.
     *
     * @param controller {@link AdaptiveToolbarButtonController} to which the buttons will be added.
     * @param trackerSupplier {@link Tracker} supplier buttons need for instantiation.
     */
    void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller,
            Supplier<@Nullable Tracker> trackerSupplier);

    /**
     * Filter the segmentation results and pick the one to display on the UI.
     *
     * @param segmentationResults Input prediction results.
     * @return The ID of the picked button.
     */
    @AdaptiveToolbarButtonVariant
    int resultFilter(List<@AdaptiveToolbarButtonVariant Integer> segmentationResults);

    /**
     * Whether the manually chosen button type can be displayed. The button may need to be hidden if
     * invalid or duplicated with custom action button on Custom Tabs.
     *
     * @param manualOverride Manually chosen button from settings.
     */
    boolean canShowManualOverride(@AdaptiveToolbarButtonVariant int manualOverride);

    /**
     * Default implementation of {@link AdaptiveToolbarBehavior} that takes into the device form
     * factor into account. Used for tabbed chrome browser and its settings UI, also in tests.
     *
     * @param context {@link Context} object.
     */
    static AdaptiveToolbarBehavior getDefaultBehavior(Context context) {
        return new AdaptiveToolbarBehavior() {
            @Override
            public void registerPerSurfaceButtons(
                    AdaptiveToolbarButtonController controller,
                    Supplier<@Nullable Tracker> trackerSupplier) {
                // Not implemented by design. Default behavior object is used for
                // AdaptiveToolbarStatePredictor while this method is used by
                // AdaptiveToolbarUiCoordinator only.
            }

            @Override
            public int resultFilter(List<Integer> segmentationResults) {
                return defaultResultFilter(context, segmentationResults);
            }

            @Override
            public boolean canShowManualOverride(int manualOverride) {
                return true;
            }

            @Override
            public boolean useRawResults() {
                return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
            }

            @Override
            public @AdaptiveToolbarButtonVariant int getSegmentationDefault() {
                return AdaptiveToolbarFeatures.getDefaultButtonVariant(context);
            }
        };
    }

    /**
     * Default segmentation result filter that takes into the device form factor into account. This
     * filter is used for tabbed chrome browser and its settings UI.
     *
     * @param context {@link Context} object.
     * @param segmentationResults An ordered list of predicted toolbar button ID.
     * @return The top choice made from the input results.
     */
    static int defaultResultFilter(Context context, List<Integer> segmentationResults) {
        if (sValidButtons.isEmpty()) {
            sValidButtons.addAll(COMMON_BUTTONS);
            sValidButtons.add(AdaptiveToolbarButtonVariant.NEW_TAB);
            sValidButtons.add(AdaptiveToolbarButtonVariant.VOICE);
        }

        List<Integer> validResults = new ArrayList<>();
        for (int result : segmentationResults) {
            if (sValidButtons.contains(result)) validResults.add(result);
        }

        if (validResults.isEmpty()) return AdaptiveToolbarButtonVariant.UNKNOWN;

        // On phones, return the top choice.
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            return validResults.get(0);
        }

        // Exclude NTB and Bookmarks from segmentation results on tablets since these buttons
        // are available on top chrome (on tab strip and omnibox).
        for (int result : validResults) {
            if (AdaptiveToolbarButtonVariant.NEW_TAB == result
                    || AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS == result) continue;
            return result;
        }
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    /**
     * Returns {@code true} when it is acceptable to use the raw segmentation result that skips the
     * thresholds to fine-tune model distribution.
     */
    boolean useRawResults();

    /** Returns the default button variant when none of the predicted results cannot be chosen. */
    @AdaptiveToolbarButtonVariant
    int getSegmentationDefault();
}
