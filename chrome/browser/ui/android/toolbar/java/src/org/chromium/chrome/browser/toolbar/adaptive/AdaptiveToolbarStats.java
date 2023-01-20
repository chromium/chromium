// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor.UiState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility methods related to metrics collection for adaptive toolbar button. */
public class AdaptiveToolbarStats {
    // Please treat this list as append only and keep it in sync with
    // AdaptiveToolbarRadioButtonState in enums.xml.
    @IntDef({AdaptiveToolbarRadioButtonState.UNKNOWN,
            AdaptiveToolbarRadioButtonState.AUTO_WITH_NEW_TAB,
            AdaptiveToolbarRadioButtonState.AUTO_WITH_SHARE,
            AdaptiveToolbarRadioButtonState.AUTO_WITH_VOICE,
            AdaptiveToolbarRadioButtonState.NEW_TAB, AdaptiveToolbarRadioButtonState.SHARE,
            AdaptiveToolbarRadioButtonState.VOICE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface AdaptiveToolbarRadioButtonState {
        int UNKNOWN = 0;
        int AUTO_WITH_NEW_TAB = 1;
        int AUTO_WITH_SHARE = 2;
        int AUTO_WITH_VOICE = 3;
        int NEW_TAB = 4;
        int SHARE = 5;
        int VOICE = 6;
        int NUM_ENTRIES = 7;
    }

    /**
     * Called to record the selected radio button on the adaptive toolbar preference page.
     *
     * @param onStartup Whether this is called on startup.
     */
    public static void recordRadioButtonStateAsync(
            AdaptiveToolbarStatePredictor adaptiveToolbarStatePredictor, boolean onStartup) {
        String histogramName = onStartup ? "Android.AdaptiveToolbarButton.Settings.Startup"
                                         : "Android.AdaptiveToolbarButton.Settings.Changed";
        adaptiveToolbarStatePredictor.recomputeUiState(uiState -> {
            RecordHistogram.recordEnumeratedHistogram(histogramName,
                    getRadioButtonStateForMetrics(uiState),
                    AdaptiveToolbarRadioButtonState.NUM_ENTRIES);
        });
    }

    /**
     * Called to record the toolbar shortcut toggle button state.
     * @param onStartup Whether this is called on startup.
     */
    public static void recordToolbarShortcutToggleState(boolean onStartup) {
        String histogramName = onStartup ? "Android.AdaptiveToolbarButton.SettingsToggle.Startup"
                                         : "Android.AdaptiveToolbarButton.SettingsToggle.Changed";
        RecordHistogram.recordBooleanHistogram(
                histogramName, AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled());
    }

    /**
     * Called on startup to record the selected segment from the backend.
     */
    public static void recordSelectedSegmentFromSegmentationPlatformAsync(
            AdaptiveToolbarStatePredictor adaptiveToolbarStatePredictor) {
        adaptiveToolbarStatePredictor.readFromSegmentationPlatform(result -> {
            RecordHistogram.recordEnumeratedHistogram(
                    "SegmentationPlatform.AdaptiveToolbar.SegmentSelected.Startup", result.second,
                    AdaptiveToolbarButtonVariant.MAX_VALUE + 1);
        });
    }

    private static @AdaptiveToolbarRadioButtonState int getRadioButtonStateForMetrics(
            UiState uiState) {
        switch (uiState.preferenceSelection) {
            case AdaptiveToolbarButtonVariant.NEW_TAB:
                return AdaptiveToolbarRadioButtonState.NEW_TAB;
            case AdaptiveToolbarButtonVariant.SHARE:
                return AdaptiveToolbarRadioButtonState.SHARE;
            case AdaptiveToolbarButtonVariant.VOICE:
                return AdaptiveToolbarRadioButtonState.VOICE;
            case AdaptiveToolbarButtonVariant.AUTO:
                switch (uiState.autoButtonCaption) {
                    case AdaptiveToolbarButtonVariant.NEW_TAB:
                        return AdaptiveToolbarRadioButtonState.AUTO_WITH_NEW_TAB;
                    case AdaptiveToolbarButtonVariant.SHARE:
                        return AdaptiveToolbarRadioButtonState.AUTO_WITH_SHARE;
                    case AdaptiveToolbarButtonVariant.VOICE:
                        return AdaptiveToolbarRadioButtonState.AUTO_WITH_VOICE;
                }
        }
        assert false : "Invalid radio button state";
        return AdaptiveToolbarRadioButtonState.UNKNOWN;
    }
}
