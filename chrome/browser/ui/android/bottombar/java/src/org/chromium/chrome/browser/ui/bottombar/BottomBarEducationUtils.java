// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.BODY;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.IS_BUTTON_BAR_VISIBLE;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.IS_ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ON_NEGATIVE_CLICKED;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ON_POSITIVE_CLICKED;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.TITLE;

import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import com.google.android.material.color.HarmonizedColors;
import com.google.android.material.color.HarmonizedColorsOptions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Helpers for the bottom bar education UI and the one-time Feature Engagement backfill of saved
 * bottom bar defaults.
 */
@NullMarked
public final class BottomBarEducationUtils {
    private BottomBarEducationUtils() {}

    /**
     * Backfills whether the user has seen the bottom bar onboarding flow from the Feature
     * Engagement Tracker once initialized. Because the saved default is app-wide while the Tracker
     * is per profile, the first regular profile Tracker to answer resolves the saved default.
     *
     * <p>Runs from post-native initialization for each regular profile, usually waiting for the
     * Tracker's posted init callback. The opt-in dialog and {@link
     * BottomBarEducationCoordinator#wouldShow(Profile)} also call it right before they decide to
     * show, so they never decide on a pending pin. Does nothing unless the pin is pending.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    public static void maybeBackfillEducationSeen(Profile profile) {
        if (!BottomBarConfigUtils.shouldBackfillEducationSeen()) return;
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile.getOriginalProfile());
        if (tracker.isInitialized()) {
            backfillEducationSeenFromTracker(tracker);
        } else {
            tracker.addOnInitializedCallback(
                    success -> {
                        if (success) backfillEducationSeenFromTracker(tracker);
                    });
        }
    }

    private static void backfillEducationSeenFromTracker(Tracker tracker) {
        // Re-check in case education showed or another profile's Tracker answered while waiting
        // for Feature Engagement to initialize.
        if (!BottomBarConfigUtils.shouldBackfillEducationSeen()) return;
        if (tracker.hasEverTriggered(
                FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION, /* fromWindow= */ true)) {
            BottomBarConfigUtils.backfillEducationSeen(/* seen= */ true);
            return;
        }
        // Only release a pending default when education is enabled and allowed to show so the
        // user gets onboarding for the new default. Otherwise keep PENDING_* and check again on
        // the next start.
        if (BottomBarConfigUtils.isEducationEnabled()
                && tracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION)) {
            BottomBarConfigUtils.backfillEducationSeen(/* seen= */ false);
        }
    }

    /**
     * Returns whether the Gemini-focused education copy should be used. This requires both the
     * Finch parameter and that Gemini (GLIC) is the eligible extra action for the given profile.
     */
    static boolean shouldUseGlicVariant(Profile profile) {
        return BottomBarConfigUtils.isEducationGeminiFocused()
                && BottomBarActionEligibility.getCandidateExtraAction(profile.getOriginalProfile())
                        == ActionId.GLIC;
    }

    static @DrawableRes int getIllustrationRes(boolean glicFocused) {
        return glicFocused
                ? R.drawable.bottom_bar_education_illustration_gemini
                : R.drawable.bottom_bar_education_illustration_ergonomics;
    }

    /**
     * Creates the model for the content of the education bubble, including its own positive ("Got
     * it") and negative ("Switch back") buttons.
     */
    static PropertyModel createBubbleContentModel(
            Context context,
            boolean glicFocused,
            Runnable onSettingsClicked,
            Runnable onPositiveClicked,
            Runnable onNegativeClicked) {
        return createContentModelBuilder(
                        context,
                        glicFocused
                                ? R.string.bottom_bar_education_title_gemini
                                : R.string.bottom_bar_education_title_ergonomics,
                        glicFocused
                                ? R.string.bottom_bar_education_body_gemini
                                : R.string.bottom_bar_education_body,
                        getIllustrationRes(glicFocused),
                        onSettingsClicked)
                .with(IS_BUTTON_BAR_VISIBLE, true)
                .with(ON_POSITIVE_CLICKED, onPositiveClicked)
                .with(ON_NEGATIVE_CLICKED, onNegativeClicked)
                .build();
    }

    /**
     * Creates the model for the content of the opt-in modal dialog. The button bar is hidden
     * because the modal dialog provides its own buttons.
     */
    static PropertyModel createDialogContentModel(Context context, Runnable onSettingsClicked) {
        return createContentModelBuilder(
                        context,
                        R.string.bottom_bar_education_title_opt_in,
                        R.string.bottom_bar_education_body_opt_in,
                        getIllustrationRes(/* glicFocused= */ false),
                        onSettingsClicked)
                .with(IS_BUTTON_BAR_VISIBLE, false)
                .build();
    }

    /**
     * Inflates the education content and binds it to {@code model}. The binding isn't destroyed, so
     * {@code model} shouldn't outlive the returned view.
     */
    static View createContentView(Context context, PropertyModel model) {
        View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.bottom_bar_education_content, /* root= */ null);
        PropertyModelChangeProcessor.create(
                model, contentView, BottomBarEducationContentViewBinder::bind);
        return contentView;
    }

    /** Returns whether the illustration shows for {@code config}. */
    static boolean isIllustrationVisible(Configuration config) {
        return config.orientation != Configuration.ORIENTATION_LANDSCAPE;
    }

    private static PropertyModel.Builder createContentModelBuilder(
            Context context,
            @StringRes int titleRes,
            @StringRes int bodyRes,
            @DrawableRes int illustrationRes,
            Runnable onSettingsClicked) {
        Context drawableContext =
                illustrationRes == R.drawable.bottom_bar_education_illustration_ergonomics
                        ? createHarmonizedIllustrationContext(context)
                        : context;
        return new PropertyModel.Builder(BottomBarEducationContentProperties.ALL_KEYS)
                .with(ILLUSTRATION, drawableContext, illustrationRes)
                .with(
                        IS_ILLUSTRATION_VISIBLE,
                        isIllustrationVisible(context.getResources().getConfiguration()))
                .with(TITLE, context.getString(titleRes))
                .with(
                        BODY,
                        SpanApplier.applySpans(
                                context.getString(bodyRes),
                                new SpanInfo(
                                        "<link>",
                                        "</link>",
                                        new ChromeClickableSpan(
                                                context, _ -> onSettingsClicked.run()))));
    }

    /**
     * Returns a context in which the GM3 Aurora colors used by the ergonomics illustration are
     * harmonized with the dynamic color palette.
     */
    private static Context createHarmonizedIllustrationContext(Context context) {
        int[] auroraColors = {
            R.color.bottom_bar_education_illustration_tertiary_container_mid_energy,
            R.color.bottom_bar_education_illustration_tertiary_container_end_energy
        };
        return HarmonizedColors.wrapContextIfAvailable(
                context,
                new HarmonizedColorsOptions.Builder().setColorResourceIds(auroraColors).build());
    }
}
