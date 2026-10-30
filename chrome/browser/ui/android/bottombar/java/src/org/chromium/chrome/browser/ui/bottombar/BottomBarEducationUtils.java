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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Helpers for the bottom bar education content shared by the bubble and the opt-in dialog. */
@NullMarked
final class BottomBarEducationUtils {
    private BottomBarEducationUtils() {}

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
     * Creates the model for the content of the education bubble, including its own positive ("Show
     * me") and negative ("Switch back") buttons.
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
                        R.string.bottom_bar_education_body,
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
