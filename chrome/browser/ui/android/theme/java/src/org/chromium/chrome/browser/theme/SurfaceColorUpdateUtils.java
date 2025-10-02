// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

/** Utility class that provides color values based on feature flags enabled. */
@NullMarked
public class SurfaceColorUpdateUtils {
    private static final String TAG = "SurfaceColorUpdateUtils";

    /** Whether enable the containment on the tab group list pane. */
    public static boolean isTabGroupListContainmentEnabled() {
        return ChromeFeatureList.sGridTabSwitcherSurfaceColorUpdate.isEnabled()
                && ChromeFeatureList.sTabGroupListContainment.getValue();
    }

    /** Whether new GM3 colors are being used for the tab group colors. */
    public static boolean useNewGm3GtsTabGroupColors() {
        return ChromeFeatureList.sAndroidTabGroupsColorUpdateGm3.isEnabled()
                || ThemeModuleUtils.isForceEnableDependencies();
    }

    /**
     * Returns the placeholder color for the card view in grid tab switcher on the enabled flag and
     * incognito.
     *
     * @param context {@link Context} used to retrieve colors.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The mini-thumbnail placeholder color.
     */
    public static @ColorInt int getCardViewMiniThumbnailPlaceholderColor(
            Context context, boolean isIncognito, @Nullable @TabGroupColorId Integer colorId) {
        if (useNewGm3GtsTabGroupColors() && colorId != null) {
            return TabGroupColorPickerUtils.getTabGroupCardMiniThumbnailPlaceholderColor(
                    context, colorId, isIncognito);
        }
        if (isIncognito) {
            return context.getColor(R.color.incognito_tab_thumbnail_placeholder_color);
        }
        return SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    /**
     * Returns the text color used for the card view in grid tab switcher on the enabled flag and
     * incognito.
     *
     * @param context {@link Context} used to retrieve colors.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The text color for the number used on the tab group cards.
     */
    public static @ColorInt int getCardViewGroupNumberTextColor(
            Context context, boolean isIncognito, @Nullable @TabGroupColorId Integer colorId) {
        if (useNewGm3GtsTabGroupColors() && colorId != null) {
            return TabGroupColorPickerUtils.getTabGroupCardTextColor(context, colorId, isIncognito);
        }
        return isIncognito
                ? context.getColor(R.color.incognito_tab_tile_number_color)
                : SemanticColorUtils.getDefaultTextColor(context);
    }

    /**
     * Returns the text color used for the card view in grid tab switcher on the enabled flag and
     * incognito.
     *
     * @param context {@link Context} used to retrieve colors.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The text color for the number used on the tab group cards.
     */
    public static ColorStateList getCardViewActionButtonColor(
            Context context, boolean isIncognito, @Nullable @TabGroupColorId Integer colorId) {
        if (useNewGm3GtsTabGroupColors() && colorId != null) {
            return ColorStateList.valueOf(
                    TabGroupColorPickerUtils.getTabGroupCardTextColor(
                            context, colorId, isIncognito));
        }
        return isIncognito
                ? AppCompatResources.getColorStateList(
                        context, R.color.incognito_tab_action_button_color)
                : ColorStateList.valueOf(
                        MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG));
    }

    /**
     * Returns the color selected icons in hub pane switcher, based on the enabled flag and
     * incognito.
     *
     * @param context {@link Context} used to retrieve colors.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isGtsUpdateEnabled Whether GTS display update is enforced or not.
     */
    public static @ColorInt int getHubPaneSwitcherSelectedIconColor(
            Context context, boolean isIncognito, boolean isGtsUpdateEnabled) {
        if (isGtsUpdateEnabled) {
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.default_icon_color_light)
                    : SemanticColorUtils.getDefaultIconColor(context);
        }

        return isIncognito
                ? ContextCompat.getColor(context, R.color.default_control_color_active_dark)
                : SemanticColorUtils.getDefaultIconColorAccent1(context);
    }
}
