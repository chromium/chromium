// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT;

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
            return new PreferenceStyle(mOuterRadius, mOuterRadius, DEFAULT, DEFAULT);
        } else if (isTop) {
            return new PreferenceStyle(mOuterRadius, mInnerRadius, DEFAULT, DEFAULT);
        } else if (isBottom) {
            return new PreferenceStyle(mInnerRadius, mOuterRadius, DEFAULT, DEFAULT);
        } else {
            return new PreferenceStyle(mInnerRadius, mInnerRadius, DEFAULT, DEFAULT);
        }
    }

    private PreferenceStyle getPreferenceStyleForCustomPreference(Preference preference) {
        if (preference instanceof PreferenceCategory) {
            return PreferenceStyle.EMPTY;
        }
        if (preference instanceof CustomStyledPreference customStyledPreference) {
            if (customStyledPreference.getCustomBackgroundStyle() == BackgroundStyle.CARD) {
                return new PreferenceStyle(
                        mOuterRadius,
                        mOuterRadius,
                        customStyledPreference.getCustomTopMargin(),
                        customStyledPreference.getCustomBottomMargin());
            }
        }

        return PreferenceStyle.EMPTY;
    }
}
