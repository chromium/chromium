// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import androidx.annotation.IntDef;
import androidx.annotation.StringDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.glic.GlicButtonStateController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility class for recording Android Bottom Bar metrics. */
@NullMarked
public class BottomBarMetrics {
    // LINT.IfChange(AndroidBottomBarPromoEvent)
    /** Events associated with the display of the bottom bar promo dialog. */
    @IntDef({PromoEvent.SHOWN, PromoEvent.ACCEPTED, PromoEvent.DISMISSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PromoEvent {
        int SHOWN = 0;
        int ACCEPTED = 1;
        int DISMISSED = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarPromoEvent)

    // LINT.IfChange(AndroidBottomBarIphEvent)
    /** Events representing showing or dismissing Bottom Bar In-Product Helps (IPH). */
    @IntDef({IphEvent.SHOWN, IphEvent.DISMISSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IphEvent {
        int SHOWN = 0;
        int DISMISSED = 1;
        int COUNT = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarIphEvent)

    // LINT.IfChange(AndroidBottomBarIphFeature)
    @StringDef({IphFeature.GLIC, IphFeature.NEW_TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IphFeature {
        String GLIC = "Glic";
        String NEW_TAB = "NewTab";
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/histograms.xml:AndroidBottomBarIphFeature)

    // LINT.IfChange(AndroidBottomBarGlicButtonState)
    /** States representing the visual state of the Gemini/Glic button inside the bottom bar. */
    @IntDef({
        GlicButtonState.DEFAULT,
        GlicButtonState.CHAT_ACTIVE,
        GlicButtonState.TASK_IN_PROGRESS,
        GlicButtonState.TASK_NEEDS_REVIEW,
        GlicButtonState.DONE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface GlicButtonState {
        int DEFAULT = 0;
        int CHAT_ACTIVE = 1;
        int TASK_IN_PROGRESS = 2;
        int TASK_NEEDS_REVIEW = 3;
        int DONE = 4;
        int COUNT = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarGlicButtonState)

    // LINT.IfChange(AndroidBottomBarGlicConvoResult)
    /** Possible results when the user interacts with the Glic toggle in the bottom bar. */
    @IntDef({GlicConvoResult.OPENED_SHEET, GlicConvoResult.CLOSED_SHEET})
    @Retention(RetentionPolicy.SOURCE)
    public @interface GlicConvoResult {
        int OPENED_SHEET = 0;
        int CLOSED_SHEET = 1;
        int COUNT = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarGlicConvoResult)

    // LINT.IfChange(AndroidBottomBarCandidateAction)
    /** Candidate action resolved for the bottom bar's extra button container slot. */
    @IntDef({CandidateAction.NONE, CandidateAction.GLIC, CandidateAction.DEPRECATED_AIM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CandidateAction {
        int NONE = 0;
        int GLIC = 1;
        int DEPRECATED_AIM = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarCandidateAction)

    // LINT.IfChange(AndroidBottomBarGlicIneligibilityReason)
    /** Reasons why Glic was determined ineligible or not shown in the bottom bar. */
    @IntDef({
        GlicIneligibilityReason.PROFILE_INELIGIBLE,
        GlicIneligibilityReason.DEPRECATED_COUNTRY_GEOFENCED,
        GlicIneligibilityReason.USER_DISABLED_IN_SETTINGS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface GlicIneligibilityReason {
        int PROFILE_INELIGIBLE = 0;
        int DEPRECATED_COUNTRY_GEOFENCED = 1;
        int USER_DISABLED_IN_SETTINGS = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarGlicIneligibilityReason)

    /**
     * Records the visual state of the Glic button in the bottom bar at the moment of the user
     * click.
     *
     * @param state The state of the Glic button state controller.
     * @param isPanelOpen Whether the Glic sheet/panel is currently open.
     */
    public static void recordGlicButtonState(
            @GlicButtonStateController.ButtonState int state, boolean isPanelOpen) {
        @GlicButtonState int histogramValue;
        switch (state) {
            case GlicButtonStateController.ButtonState.WORKING:
                histogramValue = GlicButtonState.TASK_IN_PROGRESS;
                break;
            case GlicButtonStateController.ButtonState.NEEDS_REVIEW:
                histogramValue = GlicButtonState.TASK_NEEDS_REVIEW;
                break;
            case GlicButtonStateController.ButtonState.DONE:
                histogramValue = GlicButtonState.DONE;
                break;
            case GlicButtonStateController.ButtonState.DEFAULT:
            default:
                histogramValue =
                        isPanelOpen ? GlicButtonState.CHAT_ACTIVE : GlicButtonState.DEFAULT;
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.Glic.ButtonStateOnClick", histogramValue, GlicButtonState.COUNT);
    }

    /**
     * Records the panel toggle result when the user clicks the Glic button.
     *
     * @param isPanelOpen Whether the panel was open prior to the button click (indicating a close
     *     action).
     */
    public static void recordGlicConvoResult(boolean isPanelOpen) {
        @GlicConvoResult
        int convoResult = isPanelOpen ? GlicConvoResult.CLOSED_SHEET : GlicConvoResult.OPENED_SHEET;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.Glic.ConvoResult", convoResult, GlicConvoResult.COUNT);
    }

    /**
     * Records introductory promo dialog events.
     *
     * @param event The promo event (Shown, Accepted, or Dismissed).
     */
    public static void recordPromoEvent(@PromoEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.Promo.Event", event, PromoEvent.COUNT);
    }

    /**
     * Records In-Product Help (IPH) trigger events for Bottom Bar actions.
     *
     * @param event The IPH event (Shown or Dismissed).
     * @param featureType The feature type suffix for the histogram (e.g. "Glic", "NewTab").
     */
    public static void recordIphEvent(@IphEvent int event, @IphFeature String featureType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.IPH." + featureType + ".Event", event, IphEvent.COUNT);
    }

    /**
     * Records the time elapsed from when the bottom bar becomes visible to when the Glic button is
     * successfully shown.
     *
     * @param durationMs The elapsed time in milliseconds.
     */
    public static void recordGlicTimeToAppear(long durationMs) {
        RecordHistogram.recordLongTimesHistogram(
                "Android.BottomBar.GlicTimeToAppearSinceBottomBarShown", durationMs);
    }

    /**
     * Records the decision and processing time taken to evaluate candidate extra action resolution
     * in the bottom bar.
     *
     * @param durationMs The elapsed time in milliseconds.
     */
    public static void recordCandidateDecisionTime(long durationMs) {
        RecordHistogram.recordTimesHistogram(
                "Android.BottomBar.ExtraAction.CandidateDecisionTime", durationMs);
    }

    /**
     * Records the candidate extra action resolved for the bottom bar.
     *
     * @param candidateAction The candidate extra action (None or Glic).
     */
    public static void recordCandidateExtraAction(@CandidateAction int candidateAction) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.ExtraAction.CandidateResolved",
                candidateAction,
                CandidateAction.COUNT);
    }

    /**
     * Records the reason why Glic was determined ineligible or not shown in the bottom bar.
     *
     * @param reason The ineligibility reason.
     */
    public static void recordGlicIneligibilityReason(@GlicIneligibilityReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.Glic.IneligibilityReason",
                reason,
                GlicIneligibilityReason.COUNT);
    }
}
