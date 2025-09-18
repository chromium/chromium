// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.components.browser_ui.settings.CustomStyledContainer.DEFAULT_COLOR;
import static org.chromium.components.browser_ui.settings.CustomStyledContainer.DEFAULT_MARGIN;
import static org.chromium.components.browser_ui.styles.ChromeColors.getSettingsContainerBackgroundColor;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.CustomStyledContainer.BackgroundStyle;

import java.util.ArrayList;

/**
 * Controller that assigns styling to items in a settings screen.
 *
 * <p>The controller's main responsibility is to process a list of items and determine the style for
 * each one based on its position within a "styling section." A section is a contiguous block of
 * standard items. Special items that require custom styling (see {@link #isCustomStyledPreference})
 * act as delimiters that break up these sections.
 *
 * <p>For a standard item, the controller determines if it's at the top, middle, bottom, or is a
 * standalone item in its section. This position is then used to create a default style with the
 * correct corner radii and margins.
 *
 * <p>Custom preferences are handled separately, allowing them to override the default style.
 */
@NullMarked
public class SettingsStylingController {
    private final float mDefaultRadius;
    private final float mInnerRadius;
    private final int mDefaultVerticalMargin;
    private final int mDefaultHorizontalMargin;
    private final int mSectionBottomAdditionalMargin;
    private final int mDefaultBackgroundColor;

    /**
     * Constructor for the styling controller.
     *
     * @param context The context to get the resources from.
     */
    public SettingsStylingController(@NonNull Context context) {
        mDefaultRadius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        mInnerRadius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_inner);
        mDefaultVerticalMargin =
                context.getResources().getDimensionPixelSize(R.dimen.settings_item_vertical_margin);
        mDefaultHorizontalMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_horizontal_margin);
        mSectionBottomAdditionalMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_section_bottom_margin);
        mDefaultBackgroundColor = getSettingsContainerBackgroundColor(context);
    }

    /**
     * Traverses the preference screen and returns a list of preference styles for each visible
     * preference.
     *
     * @return A list of {@link SettingsContainerStyle} objects.
     */
    public ArrayList<SettingsContainerStyle> generatePreferenceStyles(
            ArrayList<Preference> visiblePreferences) {
        ArrayList<SettingsContainerStyle> preferenceStyles = new ArrayList<>();
        for (int i = 0; i < visiblePreferences.size(); i++) {
            preferenceStyles.add(getPreferenceStyleForPosition(visiblePreferences, i));
        }
        return preferenceStyles;
    }

    /**
     * Returns whether the given preference requires custom styling.
     *
     * @param preference The preference to check.
     * @return Whether the preference has custom styling.
     */
    private boolean isCustomStyledPreference(Preference preference) {
        if (preference instanceof CustomStyledContainer customStyledPreference) {
            return customStyledPreference.getCustomBackgroundStyle() != BackgroundStyle.STANDARD;
        }
        return preference instanceof PreferenceCategory;
    }

    private @NonNull SettingsContainerStyle getPreferenceStyleForPosition(
            ArrayList<Preference> visiblePreferences, int position) {
        Preference currentPref = visiblePreferences.get(position);

        if (isCustomStyledPreference(currentPref)) {
            return getPreferenceStyleForCustomPreference(currentPref);
        }

        Preference prefAbove = (position > 0) ? visiblePreferences.get(position - 1) : null;
        Preference prefBelow =
                (position < visiblePreferences.size() - 1)
                        ? visiblePreferences.get(position + 1)
                        : null;

        boolean isTop = (prefAbove == null) || isCustomStyledPreference(prefAbove);
        boolean isBottom = (prefBelow == null) || isCustomStyledPreference(prefBelow);

        return createBuilderWithDefaultStyle(isTop, isBottom);
    }

    private SettingsContainerStyle getPreferenceStyleForCustomPreference(Preference preference) {
        if (preference instanceof PreferenceCategory) {
            return SettingsContainerStyle.EMPTY;
        } else if (preference instanceof CustomStyledContainer customStyledPreference) {
            if (customStyledPreference.getCustomBackgroundStyle() == BackgroundStyle.CARD) {
                int topMargin = customStyledPreference.getCustomTopMargin();
                if (topMargin == DEFAULT_MARGIN) topMargin = mDefaultVerticalMargin;

                int bottomMargin = customStyledPreference.getCustomBottomMargin();
                if (bottomMargin == DEFAULT_MARGIN) {
                    bottomMargin = mDefaultVerticalMargin + mSectionBottomAdditionalMargin;
                }

                int horizontalMargin = customStyledPreference.getCustomHorizontalMargin();
                if (horizontalMargin == DEFAULT_MARGIN) {
                    horizontalMargin = mDefaultHorizontalMargin;
                }

                int backgroundColor = customStyledPreference.getCustomBackgroundColor();
                if (backgroundColor == DEFAULT_COLOR) backgroundColor = mDefaultBackgroundColor;

                return new SettingsContainerStyle.Builder()
                        .setTopRadius(mDefaultRadius)
                        .setBottomRadius(mDefaultRadius)
                        .setTopMargin(topMargin)
                        .setBottomMargin(bottomMargin)
                        .setHorizontalMargin(horizontalMargin)
                        .setBackgroundColor(backgroundColor)
                        .build();
            }
        }
        return SettingsContainerStyle.EMPTY;
    }

    private SettingsContainerStyle createBuilderWithDefaultStyle(boolean isTop, boolean isBottom) {
        float topRadius = mDefaultRadius;
        float bottomRadius = mDefaultRadius;
        int bottomMargin = mDefaultVerticalMargin;

        if (isTop && isBottom) { // Standalone
            // Standalone items have an additional bottom margin
            bottomMargin += mSectionBottomAdditionalMargin;
        } else if (isTop) { // Top
            bottomRadius = mInnerRadius;
        } else if (isBottom) { // Bottom
            // Items at the end of a section have an additional bottom margin
            topRadius = mInnerRadius;
            bottomMargin += mSectionBottomAdditionalMargin;
        } else { // Middle
            topRadius = mInnerRadius;
            bottomRadius = mInnerRadius;
        }

        return new SettingsContainerStyle.Builder()
                .setTopRadius(topRadius)
                .setBottomRadius(bottomRadius)
                .setTopMargin(mDefaultVerticalMargin)
                .setBottomMargin(bottomMargin)
                .setHorizontalMargin(mDefaultHorizontalMargin)
                .setBackgroundColor(mDefaultBackgroundColor)
                .build();
    }
}
