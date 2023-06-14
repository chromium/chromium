// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

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

    private static Pair<Boolean, Integer> sSegmentationResultsForTesting;
    private static Integer sToolbarStateForTesting;

    @Nullable
    private final AndroidPermissionDelegate mAndroidPermissionDelegate;

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
     * Constructs {@code AdaptiveToolbarStatePredictor}
     *
     * @param androidPermissionDelegate used for determining if voice search can be used
     */
    public AdaptiveToolbarStatePredictor(
            @Nullable AndroidPermissionDelegate androidPermissionDelegate) {
        mAndroidPermissionDelegate = androidPermissionDelegate;
    }

    /**
     * Called to get the updated state of the UI based on various signals.
     *
     * @param callback The callback containing the result.
     */
    public void recomputeUiState(Callback<UiState> callback) {
        if (sToolbarStateForTesting != null) {
            UiState uiState = new UiState(isValidSegment(sToolbarStateForTesting),
                    sToolbarStateForTesting, sToolbarStateForTesting, sToolbarStateForTesting);
            callback.onResult(uiState);
            return;
        }

        // Early return if the feature isn't enabled.
        if (!AdaptiveToolbarFeatures.isCustomizationEnabled()) {
            callback.onResult(new UiState(false, AdaptiveToolbarButtonVariant.UNKNOWN,
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
                    replaceVariantIfDisabled(getToolbarButtonState(toolbarToggle, manualOverride,
                            finchDefault, segmentSelectionResult, ignoreSegmentationResults)),
                    getToolbarPreferenceSelection(manualOverride),
                    replaceVariantIfDisabled(getToolbarPreferenceAutoOptionSubtitleSegment(
                            finchDefault, segmentSelectionResult, ignoreSegmentationResults)));
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
    private boolean isValidSegment(@AdaptiveToolbarButtonVariant int variant) {
        switch (variant) {
            case AdaptiveToolbarButtonVariant.NEW_TAB:
            case AdaptiveToolbarButtonVariant.SHARE:
            case AdaptiveToolbarButtonVariant.VOICE:
            case AdaptiveToolbarButtonVariant.TRANSLATE:
            case AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS:
                return true;
            case AdaptiveToolbarButtonVariant.UNKNOWN:
            case AdaptiveToolbarButtonVariant.NONE:
            case AdaptiveToolbarButtonVariant.AUTO:
            case AdaptiveToolbarButtonVariant.PRICE_TRACKING:
            case AdaptiveToolbarButtonVariant.READER_MODE:
                return false;
            default:
                assert false : "Invalid adaptive toolbar button variant: " + variant;
                return false;
        }
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
     * boolean indicating whether the backend is ready. (2) a {@link AdaptiveToolbarButtonVariant}
     * indicating which segment should be shown.
     *
     * @param callback A callback for results.
     */
    public void readFromSegmentationPlatform(Callback<Pair<Boolean, Integer>> callback) {
        if (sSegmentationResultsForTesting != null) {
            callback.onResult(sSegmentationResultsForTesting);
            return;
        }

        // TODO(shaktisahu): Try decoupling profile from this class.
        AdaptiveToolbarBridge.getSessionVariantButton(
                Profile.getLastUsedRegularProfile(), result -> callback.onResult(result));
    }

    /**
     * Returns the default segment if {@code variant} is not available on this system. Otherwise
     * returns {@code variant} unchanged.
     */
    private @AdaptiveToolbarButtonVariant int replaceVariantIfDisabled(
            @AdaptiveToolbarButtonVariant int variant) {
        if (isVariantEnabled(variant)) return variant;
        variant = AdaptiveToolbarFeatures.getSegmentationDefault();
        if (isVariantEnabled(variant)) return variant;
        // Fallback in the unlikely situation the default is disabled.
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    private boolean isVariantEnabled(@AdaptiveToolbarButtonVariant int variant) {
        switch (variant) {
            case AdaptiveToolbarButtonVariant.VOICE:
                if (mAndroidPermissionDelegate == null) return true;
                return VoiceRecognitionUtil.isVoiceSearchEnabled(mAndroidPermissionDelegate);
            case AdaptiveToolbarButtonVariant.TRANSLATE:
                return AdaptiveToolbarFeatures.isAdaptiveToolbarTranslateEnabled();
            case AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS:
                return AdaptiveToolbarFeatures.isAdaptiveToolbarAddToBookmarksEnabled();
            default:
                return true;
        }
    }

    /**
     * Conversion method between {@link SegmentId} and {@link
     * AdaptiveToolbarButtonVariant}.
     */
    public static @AdaptiveToolbarButtonVariant int getAdaptiveToolbarButtonVariantFromSegmentId(
            SegmentId segmentId) {
        switch (segmentId) {
            case OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
                return AdaptiveToolbarButtonVariant.NEW_TAB;
            case OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
                return AdaptiveToolbarButtonVariant.SHARE;
            case OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
                return AdaptiveToolbarButtonVariant.VOICE;
            case OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING:
                return AdaptiveToolbarButtonVariant.PRICE_TRACKING;
            default:
                return AdaptiveToolbarButtonVariant.UNKNOWN;
        }
    }

    /** For testing only. */
    @VisibleForTesting
    public static void setSegmentationResultsForTesting(Pair<Boolean, Integer> results) {
        sSegmentationResultsForTesting = results;
    }

    /** For testing only. */
    @VisibleForTesting
    public static void setToolbarStateForTesting(Integer toolbarState) {
        sToolbarStateForTesting = toolbarState;
    }
}
