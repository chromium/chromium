// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;

/** Controller to assign styling to preferences in a settings screen. */
@NullMarked
public class SettingsStylingController {
    /** A class that holds the background style details for a preference screen. */
    public static class BackgroundStyleDetails {
        public final float topRadius;
        public final float bottomRadius;

        /** An empty style with no background. */
        public static final BackgroundStyleDetails EMPTY = new BackgroundStyleDetails(0, 0);

        private BackgroundStyleDetails(float topRadius, float bottomRadius) {
            this.topRadius = topRadius;
            this.bottomRadius = bottomRadius;
        }
    }

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
     * Traverses the preference screen and returns a list of background style details for each
     * visible preference.
     *
     * @return A list of {@link BackgroundStyleDetails} objects.
     */
    public ArrayList<BackgroundStyleDetails> generateBackgroundStyleDetails() {
        ArrayList<Preference> visiblePreferences = getVisiblePreferences();
        ArrayList<BackgroundStyleDetails> backgroundStyles = new ArrayList<>();
        for (int i = 0; i < visiblePreferences.size(); i++) {
            backgroundStyles.add(getBackgroundStyleDetailsForPosition(visiblePreferences, i));
        }
        return backgroundStyles;
    }

    private ArrayList<Preference> getVisiblePreferences() {
        ArrayList<Preference> visiblePreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceScreen.getPreferenceCount(); i++) {
            Preference preference = mPreferenceScreen.getPreference(i);
            if (preference.isVisible()) {
                visiblePreferences.add(preference);
            }
        }
        return visiblePreferences;
    }

    private @NonNull BackgroundStyleDetails getBackgroundStyleDetailsForPosition(
            ArrayList<Preference> visiblePreferences, int position) {

        Preference currentPref = visiblePreferences.get(position);
        if (currentPref instanceof PreferenceCategory) {
            return BackgroundStyleDetails.EMPTY;
        }

        Preference prefAbove = (position > 0) ? visiblePreferences.get(position - 1) : null;
        Preference prefBelow =
                (position < visiblePreferences.size() - 1)
                        ? visiblePreferences.get(position + 1)
                        : null;

        boolean isTop = (prefAbove == null) || (prefAbove instanceof PreferenceCategory);
        boolean isBottom = (prefBelow == null) || (prefBelow instanceof PreferenceCategory);

        if (isTop && isBottom) {
            return new BackgroundStyleDetails(mOuterRadius, mOuterRadius);
        } else if (isTop) {
            return new BackgroundStyleDetails(mOuterRadius, mInnerRadius);
        } else if (isBottom) {
            return new BackgroundStyleDetails(mInnerRadius, mOuterRadius);
        } else {
            return new BackgroundStyleDetails(mInnerRadius, mInnerRadius);
        }
    }
}
