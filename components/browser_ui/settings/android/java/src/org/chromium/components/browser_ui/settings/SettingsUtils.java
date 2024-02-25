// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.app.Activity;
import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.StrictMode;
import android.view.View;
import android.view.ViewTreeObserver.OnScrollChangedListener;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.XmlRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.Toolbar;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.util.ToolbarUtils;

/** A helper class for Settings. */
public class SettingsUtils {
    /**
     * A helper that is used to load preferences from XML resources without causing a
     * StrictModeViolation. See http://crbug.com/692125.
     *
     * @param preferenceFragment A Support Library {@link PreferenceFragmentCompat}.
     * @param preferencesResId   The id of the XML resource to add to the PreferenceFragment.
     */
    public static void addPreferencesFromResource(
            PreferenceFragmentCompat preferenceFragment, @XmlRes int preferencesResId) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            preferenceFragment.addPreferencesFromResource(preferencesResId);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Returns a view tree observer to show the shadow if and only if the view is scrolled.
     * @param view   The view whose scroll will be detected to determine the shadow's visibility.
     * @param shadow The shadow to show/hide.
     * @return An OnScrollChangedListener that detects scrolling and shows the passed in shadow
     *         when a scroll is detected and hides the shadow otherwise.
     */
    public static OnScrollChangedListener getShowShadowOnScrollListener(View view, View shadow) {
        return new OnScrollChangedListener() {
            @Override
            public void onScrollChanged() {
                if (!view.canScrollVertically(-1)) {
                    shadow.setVisibility(View.GONE);
                } else {
                    shadow.setVisibility(View.VISIBLE);
                }
            }
        };
    }

    /** Creates a {@link Drawable} for the given resource id with the default icon color applied. */
    public static Drawable getTintedIcon(Context context, @DrawableRes int resId) {
        return getTintedIcon(context, resId, R.color.default_icon_color_tint_list);
    }

    /** Creates a {@link Drawable} for the given resource id with provided color id applied. */
    public static Drawable getTintedIcon(
            Context context, @DrawableRes int resId, @ColorRes int colorId) {
        Drawable icon = AppCompatResources.getDrawable(context, resId);
        // DrawableCompat.setTint() doesn't work well on BitmapDrawables on older versions.
        icon.setColorFilter(
                AppCompatResources.getColorStateList(context, colorId).getDefaultColor(),
                PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /**
     * A helper that is used to set the visibility of the overflow menu view in a given activity.
     *
     * @param activity The Activity containing the action bar with the menu.
     * @param visibility The Activity containing the action bar with the menu.
     * @return True if the visibility could be set, false otherwise (e.g. because no menu exists).
     */
    public static boolean setOverflowMenuVisibility(@Nullable Activity activity, int visibility) {
        if (activity == null) return false;
        Toolbar toolbar = activity.findViewById(R.id.action_bar);
        if (toolbar == null) return false;
        ToolbarUtils.setOverflowMenuVisibility(toolbar, visibility);
        return true;
    }
}
