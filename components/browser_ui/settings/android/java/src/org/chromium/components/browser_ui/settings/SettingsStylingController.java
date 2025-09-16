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
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.CustomStyledContainer.BackgroundStyle;

import java.util.ArrayList;

/** Controller to assign styling to preferences in a settings screen. */
@NullMarked
public class SettingsStylingController {

    private final PreferenceScreen mPreferenceScreen;
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
     * @param preferenceScreen The preference screen to be styled.
     */
    public SettingsStylingController(
            @NonNull Context context, @NonNull PreferenceScreen preferenceScreen) {
        mPreferenceScreen = preferenceScreen;
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
        mDefaultBackgroundColor =
                getSettingsContainerBackgroundColor(mPreferenceScreen.getContext());
    }

    /**
     * Traverses the preference screen and returns a list of preference styles for each visible
     * preference.
     *
     * @return A list of {@link SettingsContainerStyle} objects.
     */
    public ArrayList<SettingsContainerStyle> generatePreferenceStyles() {
        ArrayList<Preference> visiblePreferences = getVisiblePreferences();
        ArrayList<SettingsContainerStyle> preferenceStyles = new ArrayList<>();
        for (int i = 0; i < visiblePreferences.size(); i++) {
            preferenceStyles.add(getPreferenceStyleForPosition(visiblePreferences, i));
        }
        return preferenceStyles;
    }

    private ArrayList<Preference> getVisiblePreferences() {
        ArrayList<Preference> visiblePreferences = new ArrayList<>();
        if (mPreferenceScreen == null) return visiblePreferences;

        for (int i = 0; i < mPreferenceScreen.getPreferenceCount(); i++) {
            Preference preference = mPreferenceScreen.getPreference(i);
            if (preference.isVisible()) {
                addVisiblePreferences(preference, visiblePreferences);
            }
        }
        return visiblePreferences;
    }

    /**
     * Recursively adds all visible preferences.
     *
     * @param preference The preference to start from.
     * @param visiblePreferences The list to add visible preferences to.
     */
    private void addVisiblePreferences(
            Preference preference, ArrayList<Preference> visiblePreferences) {
        visiblePreferences.add(preference);
        if (preference instanceof PreferenceGroup preferenceGroup) {
            for (int i = 0; i < preferenceGroup.getPreferenceCount(); i++) {
                Preference nestedPreference = preferenceGroup.getPreference(i);
                if (nestedPreference.isVisible()) {
                    addVisiblePreferences(nestedPreference, visiblePreferences);
                }
            }
        }
    }

    /**
     * Returns whether the given preference requires custom styling.
     *
     * @param preference The preference to check.
     * @return Whether the preference has custom styling.
     */
    private boolean hasCustomStyling(Preference preference) {
        if (preference instanceof CustomStyledContainer customStyledPreference) {
            return customStyledPreference.getCustomBackgroundStyle() != BackgroundStyle.STANDARD;
        }
        return preference instanceof PreferenceCategory;
    }

    private @NonNull SettingsContainerStyle getPreferenceStyleForPosition(
            ArrayList<Preference> visiblePreferences, int position) {
        Preference currentPref = visiblePreferences.get(position);

        if (hasCustomStyling(currentPref)) {
            return getPreferenceStyleForCustomPreference(currentPref);
        }

        Preference prefAbove = (position > 0) ? visiblePreferences.get(position - 1) : null;
        Preference prefBelow =
                (position < visiblePreferences.size() - 1)
                        ? visiblePreferences.get(position + 1)
                        : null;

        float topRadius = mDefaultRadius;
        float bottomRadius = mDefaultRadius;
        int bottomMargin = mDefaultVerticalMargin;

        boolean isTop = (prefAbove == null) || hasCustomStyling(prefAbove);
        boolean isBottom = (prefBelow == null) || hasCustomStyling(prefBelow);

        if (isTop && isBottom) {
            // Standalone items have an additional bottom margin
            bottomMargin = mSectionBottomAdditionalMargin + mDefaultVerticalMargin;
        } else if (isTop) {
            bottomRadius = mInnerRadius;
        } else if (isBottom) {
            // Items at the end of a section have an additional bottom margin
            bottomMargin = mSectionBottomAdditionalMargin + mDefaultVerticalMargin;
            topRadius = mInnerRadius;
        } else {
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
}
