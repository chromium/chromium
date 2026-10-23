// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.res.Configuration;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.color.HarmonizedColors;
import com.google.android.material.color.HarmonizedColorsOptions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Helpers for bottom bar user education views and strings. */
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
     * Inflates the content of the education bubble, including its own positive ("Show me") and
     * negative ("Switch back") buttons.
     */
    // TODO(crbug.com/567168121): Refactor to use a ViewBinder and follow MVC.
    static View inflateBubbleContent(
            Context context,
            boolean glicFocused,
            Runnable onSettingsClicked,
            Runnable onPositiveClicked,
            Runnable onNegativeClicked) {
        View contentView =
                inflateContent(
                        context,
                        glicFocused
                                ? R.string.bottom_bar_education_title_gemini
                                : R.string.bottom_bar_education_title_ergonomics,
                        R.string.bottom_bar_education_body,
                        getIllustrationRes(glicFocused),
                        onSettingsClicked);
        contentView
                .findViewById(R.id.positive_button)
                .setOnClickListener(v -> onPositiveClicked.run());
        contentView
                .findViewById(R.id.negative_button)
                .setOnClickListener(v -> onNegativeClicked.run());
        return contentView;
    }

    /**
     * Inflates the content of the opt-in modal dialog. The button bar is hidden because the modal
     * dialog provides its own buttons.
     */
    static View inflateDialogContent(Context context, Runnable onSettingsClicked) {
        View contentView =
                inflateContent(
                        context,
                        R.string.bottom_bar_education_title_opt_in,
                        R.string.bottom_bar_education_body_opt_in,
                        getIllustrationRes(/* glicFocused= */ false),
                        onSettingsClicked);
        contentView.findViewById(R.id.button_bar).setVisibility(View.GONE);
        return contentView;
    }

    static void updateIllustrationVisibility(View contentView, Configuration config) {
        contentView
                .findViewById(R.id.education_illustration)
                .setVisibility(
                        config.orientation == Configuration.ORIENTATION_LANDSCAPE
                                ? View.GONE
                                : View.VISIBLE);
    }

    private static View inflateContent(
            Context context,
            @StringRes int titleRes,
            @StringRes int bodyRes,
            @DrawableRes int illustrationRes,
            Runnable onSettingsClicked) {
        View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.bottom_bar_education_content, /* root= */ null);

        Context drawableContext =
                illustrationRes == R.drawable.bottom_bar_education_illustration_ergonomics
                        ? createHarmonizedIllustrationContext(context)
                        : context;

        ImageView illustration = contentView.findViewById(R.id.education_illustration);
        illustration.setImageDrawable(
                AppCompatResources.getDrawable(drawableContext, illustrationRes));

        updateIllustrationVisibility(contentView, context.getResources().getConfiguration());

        TextView title = contentView.findViewById(R.id.education_title);
        title.setText(titleRes);

        TextView body = contentView.findViewById(R.id.education_body);
        body.setText(
                SpanApplier.applySpans(
                        context.getString(bodyRes),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(context, v -> onSettingsClicked.run()))));
        body.setMovementMethod(LinkMovementMethod.getInstance());
        return contentView;
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
