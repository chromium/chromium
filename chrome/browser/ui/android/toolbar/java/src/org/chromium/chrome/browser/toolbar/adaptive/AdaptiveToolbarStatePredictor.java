// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;

/**
 * Central class that determines the state of the toolbar button based on finch configuration,
 * user preference, and segmentation platform backend prediction. This class is used only for the
 * segmentation experiment.
 */
public class AdaptiveToolbarStatePredictor {
    /**
     * Key used to lookup segmentation results for adaptive toolbar. Must be kept in sync with
     * components/segmentation_platform/internal/constants.cc.
     */
    private static final String ADAPTIVE_TOOLBAR_SEGMENTATION_KEY = "adaptive_toolbar";

    private static Pair<Boolean, Integer> sSegmentationResultsForTesting;

    /**
     * The result of the predictor. Contains the UI states specific to the toolbar button.
     */
    public static class UiState {
        /**
         * Used to determine whether we can show any toolbar shortcut specific UI.
         */
        public final boolean canShowUi;

        /**
         * Used for showing the toolbar shortcut action in the toolbar UI.
         */
        public final @AdaptiveToolbarButtonVariant int toolbarButtonState;

        /**
         * Used for the selected radio button in the toolbar shortcut settings page.
         */
        public final @AdaptiveToolbarButtonVariant int preferenceSelection;

        /**
         * Used for the substring used in the auto option.
         */
        public final @AdaptiveToolbarButtonVariant int autoButtonCaption;

        /**
         * Constructor.
         */
        public UiState(boolean canShowUi, int toolbarButtonState, int preferenceSelection,
                int autoButtonCaption) {
            this.canShowUi = canShowUi;
            this.toolbarButtonState = toolbarButtonState;
            this.preferenceSelection = preferenceSelection;
            this.autoButtonCaption = autoButtonCaption;
        }
    }

    /**
     * Called to get the updated state of the UI based on various signals.
     *
     * @param callback The callback containing the result.
     */
    public void recomputeUiState(Callback<UiState> callback) {
        // Early return if the feature isn't enabled.
        if (!AdaptiveToolbarFeatures.isCustomizationEnabled()) {
            boolean canShowUi = AdaptiveToolbarFeatures.isSingleVariantModeEnabled();
            int toolbarButtonState = AdaptiveToolbarFeatures.isSingleVariantModeEnabled()
                    ? AdaptiveToolbarFeatures.getSingleVariantMode()
                    : AdaptiveToolbarButtonVariant.UNKNOWN;
            callback.onResult(new UiState(canShowUi, toolbarButtonState,
                    AdaptiveToolbarButtonVariant.UNKNOWN, AdaptiveToolbarButtonVariant.UNKNOWN));
            return;
        }

        int manualOverride = readManualOverrideFromPrefs();
        int finchDefault = AdaptiveToolbarFeatures.getSegmentationDefault();
        boolean toolbarToggle = readToolbarToggleStateFromPrefs();
        boolean ignoreSegmentationResults = AdaptiveToolbarFeatures.ignoreSegmentationResults();
        readFromSegmentationPlatform(segmentationResult -> {
            boolean isReady = segmentationResult.first;
            int segmentSelectionResult = segmentationResult.second;
            UiState uiState = new UiState(canShowUi(isReady),
                    getToolbarButtonState(toolbarToggle, manualOverride, finchDefault,
                            segmentSelectionResult, ignoreSegmentationResults),
                    getToolbarPreferenceSelection(manualOverride),
                    getToolbarPreferenceAutoOptionSubtitleSegment(
                            finchDefault, segmentSelectionResult, ignoreSegmentationResults));
            callback.onResult(uiState);
        });
    }

    private @AdaptiveToolbarButtonVariant int getToolbarButtonState(boolean toolbarToggle,
            @AdaptiveToolbarButtonVariant int manualOverride,
            @AdaptiveToolbarButtonVariant int finchDefault,
            @AdaptiveToolbarButtonVariant int segmentationResult,
            boolean ignoreSegmentationResult) {
        if (!toolbarToggle) return AdaptiveToolbarButtonVariant.UNKNOWN;
        if (isValidSegment(manualOverride)) return manualOverride;
        if (ignoreSegmentationResult) return finchDefault;

        return isValidSegment(segmentationResult) ? segmentationResult : finchDefault;
    }

    private @AdaptiveToolbarButtonVariant int getToolbarPreferenceSelection(
            @AdaptiveToolbarButtonVariant int manualOverride) {
        if (isValidSegment(manualOverride)) return manualOverride;
        return AdaptiveToolbarButtonVariant.AUTO;
    }

    private @AdaptiveToolbarButtonVariant int getToolbarPreferenceAutoOptionSubtitleSegment(
            @AdaptiveToolbarButtonVariant int finchDefault,
            @AdaptiveToolbarButtonVariant int segmentationResult,
            boolean ignoreSegmentationResult) {
        return ignoreSegmentationResult
                ? finchDefault
                : (isValidSegment(segmentationResult) ? segmentationResult : finchDefault);
    }

    /**
     * @return Given a segment, whether it is a valid segment that can be shown to the user.
     */
    private boolean isValidSegment(@AdaptiveToolbarButtonVariant int segment) {
        if (segment == AdaptiveToolbarButtonVariant.UNKNOWN) return false;
        return segment == AdaptiveToolbarButtonVariant.NEW_TAB
                || segment == AdaptiveToolbarButtonVariant.SHARE
                || segment == AdaptiveToolbarButtonVariant.VOICE;
    }

    private boolean canShowUi(boolean isReady) {
        boolean isFeatureEnabled = AdaptiveToolbarFeatures.isCustomizationEnabled();
        boolean isUiEnabled = !AdaptiveToolbarFeatures.disableUi();
        boolean showUiOnlyAfterReady = AdaptiveToolbarFeatures.showUiOnlyAfterReady();
        return isFeatureEnabled && isUiEnabled && (!showUiOnlyAfterReady || isReady);
    }

    @VisibleForTesting
    @AdaptiveToolbarButtonVariant
    int readManualOverrideFromPrefs() {
        return AdaptiveToolbarPrefs.getCustomizationSetting();
    }

    @VisibleForTesting
    boolean readToolbarToggleStateFromPrefs() {
        return AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled();
    }

    /**
     * Called to read results from the segmentation backend. The result contains a pair of (1) a
     * boolean indicating whether the backend is ready. (2) a {@link @AdaptiveToolbarButtonVariant}
     * indicating which segment should be shown.
     *
     * @param callback A callback for results.
     */
    @VisibleForTesting
    void readFromSegmentationPlatform(Callback<Pair<Boolean, Integer>> callback) {
        if (sSegmentationResultsForTesting != null) {
            callback.onResult(sSegmentationResultsForTesting);
            return;
        }

        // TODO(shaktisahu): Try decoupling profile from this class.
        SegmentationPlatformService segmentationPlatformService =
                SegmentationPlatformServiceFactory.getForProfile(
                        Profile.getLastUsedRegularProfile());
        segmentationPlatformService.getSelectedSegment(
                ADAPTIVE_TOOLBAR_SEGMENTATION_KEY, result -> {
                    callback.onResult(new Pair<>(result.isReady,
                            getAdaptiveToolbarButtonVariantFromOptimizationTarget(
                                    result.selectedSegment)));
                });
    }

    /**
     * Conversion method between {@link OptimizationTarget} and {@link
     * AdaptiveToolbarButtonVariant}.
     */
    @VisibleForTesting
    static @AdaptiveToolbarButtonVariant int getAdaptiveToolbarButtonVariantFromOptimizationTarget(
            OptimizationTarget optimizationTarget) {
        switch (optimizationTarget) {
            case OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
                return AdaptiveToolbarButtonVariant.NEW_TAB;
            case OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
                return AdaptiveToolbarButtonVariant.SHARE;
            case OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
                return AdaptiveToolbarButtonVariant.VOICE;
            default:
                return AdaptiveToolbarButtonVariant.UNKNOWN;
        }
    }

    /** For testing only. */
    @VisibleForTesting
    public static void setSegmentationResultsForTesting(Pair<Boolean, Integer> results) {
        sSegmentationResultsForTesting = results;
    }
}
