// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT_MARGIN;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.CustomStyledPreference.BackgroundStyle;

import java.util.ArrayList;

/** Controller to assign styling to preferences in a settings screen. */
@NullMarked
public class SettingsStylingController {

    private final PreferenceScreen mPreferenceScreen;
    private final float mOuterRadius;
    private final float mInnerRadius;
    private final int mDefaultVerticalMargin;
    private final int mDefaultHorizontalMargin;
    private final int mSectionBottomAdditionalMargin;

    /**
     * Constructor for the styling controller.
     *
     * @param context The context to get the resources from.
     * @param preferenceScreen The preference screen to be styled.
     */
    public SettingsStylingController(
            @NonNull Context context, @NonNull PreferenceScreen preferenceScreen) {
        mPreferenceScreen = preferenceScreen;
        mOuterRadius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_outer);
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
    }

    /**
     * Traverses the preference screen and returns a list of preference styles for each visible
     * preference.
     *
     * @return A list of {@link PreferenceStyle} objects.
     */
    public ArrayList<PreferenceStyle> generatePreferenceStyles() {
        ArrayList<Preference> visiblePreferences = getVisiblePreferences();
        ArrayList<PreferenceStyle> preferenceStyles = new ArrayList<>();
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
        return preference instanceof PreferenceCategory
                || preference instanceof CustomStyledPreference;
    }

    private @NonNull PreferenceStyle getPreferenceStyleForPosition(
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

        boolean isTop = (prefAbove == null) || hasCustomStyling(prefAbove);
        boolean isBottom = (prefBelow == null) || hasCustomStyling(prefBelow);

        if (isTop && isBottom) {
            // Standalone items have an additional bottom margin
            return new PreferenceStyle(
                    /* topRadius= */ mOuterRadius,
                    /* bottomRadius= */ mOuterRadius,
                    /* topMargin= */ mDefaultVerticalMargin,
                    /* bottomMargin= */ mSectionBottomAdditionalMargin + mDefaultVerticalMargin,
                    /* horizontalMargin= */ mDefaultHorizontalMargin);
        } else if (isTop) {
            return new PreferenceStyle(
                    /* topRadius= */ mOuterRadius,
                    /* bottomRadius= */ mInnerRadius,
                    /* topMargin= */ mDefaultVerticalMargin,
                    /* bottomMargin= */ mDefaultVerticalMargin,
                    /* horizontalMargin= */ mDefaultHorizontalMargin);
        } else if (isBottom) {
            // Items at the end of a section have an additional bottom margin
            return new PreferenceStyle(
                    /* topRadius= */ mInnerRadius,
                    /* bottomRadius= */ mOuterRadius,
                    /* topMargin= */ mDefaultVerticalMargin,
                    /* bottomMargin= */ mSectionBottomAdditionalMargin + mDefaultVerticalMargin,
                    /* horizontalMargin= */ mDefaultHorizontalMargin);
        } else {
            return new PreferenceStyle(
                    /* topRadius= */ mInnerRadius,
                    /* bottomRadius= */ mInnerRadius,
                    /* topMargin= */ mDefaultVerticalMargin,
                    /* bottomMargin= */ mDefaultVerticalMargin,
                    /* horizontalMargin= */ mDefaultHorizontalMargin);
        }
    }

    private PreferenceStyle getPreferenceStyleForCustomPreference(Preference preference) {
        if (preference instanceof PreferenceCategory) {
            return PreferenceStyle.EMPTY;
        } else if (preference instanceof CustomStyledPreference customStyledPreference) {
            if (customStyledPreference.getCustomBackgroundStyle() == BackgroundStyle.CARD) {
                int topMargin = customStyledPreference.getCustomTopMargin();
                int bottomMargin = customStyledPreference.getCustomBottomMargin();
                int horizontalMargin = customStyledPreference.getCustomHorizontalMargin();
                return new PreferenceStyle(
                        /* topRadius= */ mOuterRadius,
                        /* bottomRadius= */ mOuterRadius,
                        /* topMargin= */ topMargin != DEFAULT_MARGIN
                                ? topMargin
                                : mDefaultVerticalMargin,
                        /* bottomMargin= */ bottomMargin != DEFAULT_MARGIN
                                ? bottomMargin
                                : mDefaultVerticalMargin + mSectionBottomAdditionalMargin,
                        /* horizontalMargin= */ horizontalMargin != DEFAULT_MARGIN
                                ? horizontalMargin
                                : mDefaultHorizontalMargin);
            }
        }

        return PreferenceStyle.EMPTY;
    }
}
