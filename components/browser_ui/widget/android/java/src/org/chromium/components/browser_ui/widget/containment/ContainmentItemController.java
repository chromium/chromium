// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import static org.chromium.components.browser_ui.styles.SemanticColorUtils.getSettingsContainerBackgroundColor;
import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_COLOR;
import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_MARGIN;

import android.content.Context;
import android.graphics.Color;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem.BackgroundStyle;

import java.util.ArrayList;
import java.util.List;

/**
 * Controller that assigns styling to items in a settings screen.
 *
 * <p>This controller is responsible for generating {@link ContainerStyle} objects for both {@link
 * Preference} items and generic {@link View}s.
 *
 * <p>The core logic is based on the concept of a "styling section," which is a contiguous block of
 * standard items. Special items that require custom styling (see {@link
 * #isCustomStyledPreference(Preference)} and {@link #isCustomStyledView(View)}) act as delimiters
 * that break up these sections.
 *
 * <p>For a standard item (either a Preference or a View), the controller determines if it's at the
 * top, middle, bottom, or is a standalone item in its section. This position is then passed to
 * {@link #createBuilderWithDefaultStyle}, which creates the final style with the correct corner
 * radii and default margins.
 *
 * <p>Custom preferences are handled separately in {@link #getStyleForCustomContainer}, where their
 * own styling values are prioritized over the controller's defaults.
 */
@NullMarked
public class ContainmentItemController {
    private final float mDefaultRadius;
    private final float mInnerRadius;
    private final int mDefaultContainerVerticalMargin;
    private final int mDefaultMargin;
    private final int mSectionBottomAdditionalMargin;
    private final int mDefaultPadding;
    private final int mMultiLineVerticalPadding;
    private final int mDefaultBackgroundColor;
    static final int TRANSPARENT_BACKGROUND_COLOR = Color.TRANSPARENT;

    /**
     * Constructor for the styling controller.
     *
     * @param context The context to get the resources from.
     */
    public ContainmentItemController(@NonNull Context context) {
        mDefaultRadius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        mInnerRadius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_inner);
        mDefaultContainerVerticalMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_container_vertical_margin);
        mDefaultMargin = context.getResources().getDimensionPixelSize(R.dimen.settings_item_margin);
        mSectionBottomAdditionalMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_section_bottom_margin);
        mDefaultPadding =
                context.getResources().getDimensionPixelSize(R.dimen.settings_item_default_padding);
        mMultiLineVerticalPadding =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_vertical_padding_multi_line);
        mDefaultBackgroundColor = getSettingsContainerBackgroundColor(context);
    }

    /**
     * Generates a list of {@link ContainerStyle} for the given preferences. The style for each
     * preference is determined by its position within a "styling section."
     *
     * @param visiblePreferences The list of visible preferences on the screen.
     * @return A list of {@link ContainerStyle} objects.
     */
    public ArrayList<ContainerStyle> generatePreferenceStyles(
            ArrayList<Preference> visiblePreferences) {
        ArrayList<ContainerStyle> preferenceStyles = new ArrayList<>();
        for (int i = 0; i < visiblePreferences.size(); i++) {
            preferenceStyles.add(getPreferenceStyleForPosition(visiblePreferences, i));
        }
        return preferenceStyles;
    }

    /**
     * Generates a list of {@link ContainerStyle} objects for the given views.
     *
     * @param views The list of views to generate styles for.
     * @return A list of {@link ContainerStyle} objects.
     */
    public ArrayList<ContainerStyle> generateViewStyles(List<View> views) {
        ArrayList<ContainerStyle> styles = new ArrayList<>();
        for (int i = 0; i < views.size(); i++) {
            styles.add(getViewStyleForPosition(views, i));
        }
        return styles;
    }

    /**
     * Determines the style for a preference based on its position in the list.
     *
     * @param visiblePreferences The list of all visible preferences.
     * @param position The position of the current preference in the list.
     * @return The {@link ContainerStyle} for the preference.
     */
    private @NonNull ContainerStyle getPreferenceStyleForPosition(
            ArrayList<Preference> visiblePreferences, int position) {
        Preference currentPref = visiblePreferences.get(position);
        boolean isSingleLine = currentPref.getSummary() == null;

        if (isCustomStyledPreference(currentPref)) {
            if (currentPref instanceof PreferenceCategory) {
                return new ContainerStyle.Builder()
                        .setBottomMargin(mDefaultMargin)
                        .setHorizontalMargin(mDefaultMargin)
                        .setBackgroundColor(TRANSPARENT_BACKGROUND_COLOR)
                        .build();
            }
            return getStyleForCustomContainer((ContainmentItem) currentPref, isSingleLine);
        }

        // For standard items, styling is determined by their position within a section.

        Preference prefAbove = (position > 0) ? visiblePreferences.get(position - 1) : null;
        Preference prefBelow =
                (position < visiblePreferences.size() - 1)
                        ? visiblePreferences.get(position + 1)
                        : null;

        boolean isTop = (prefAbove == null) || isCustomStyledPreference(prefAbove);
        boolean isBottom = (prefBelow == null) || isCustomStyledPreference(prefBelow);

        return createBuilderWithDefaultStyle(isTop, isBottom, isSingleLine);
    }

    /**
     * Returns whether the given preference requires custom styling. Custom styled preferences act
     * as delimiters for styling sections.
     *
     * @param preference The preference to check.
     * @return Whether the preference has custom styling.
     */
    private boolean isCustomStyledPreference(Preference preference) {
        if (preference instanceof ContainmentItem customStyledPreference) {
            return customStyledPreference.getCustomBackgroundStyle() != BackgroundStyle.STANDARD;
        }
        return preference instanceof PreferenceCategory;
    }

    /**
     * Determines the style for a view based on its position in the list.
     *
     * @param views The list of all views to be styled.
     * @param position The position of the current view in the list.
     * @return The {@link ContainerStyle} for the view.
     */
    private ContainerStyle getViewStyleForPosition(List<View> views, int position) {
        View view = views.get(position);

        if (isCustomStyledView(view)) {
            return getStyleForCustomContainer((ContainmentItem) view, /* isSingleLine= */ true);
        }

        // For standard items, styling is determined by their position within a section.

        boolean isTop = position == 0 || isCustomStyledView(views.get(position - 1));
        boolean isBottom =
                position == views.size() - 1 || isCustomStyledView(views.get(position + 1));
        return createBuilderWithDefaultStyle(isTop, isBottom, /* isSingleLine= */ false);
    }

    /**
     * Returns whether the given view requires custom styling. Custom styled views act as delimiters
     * for styling sections.
     *
     * @param view The view to check.
     * @return Whether the view has custom styling.
     */
    private boolean isCustomStyledView(View view) {
        if (view instanceof ContainmentItem customStyledPreference) {
            return customStyledPreference.getCustomBackgroundStyle() != BackgroundStyle.STANDARD;
        }
        return false;
    }

    /**
     * Creates a {@link ContainerStyle} for a {@link ContainmentItem}. This method respects the
     * custom values provided by the container, falling back to controller defaults if they are not
     * provided.
     *
     * @param container The container to generate a style for.
     * @param isSingleLine Whether the item is single line.
     * @return The {@link ContainerStyle} for the container.
     */
    private ContainerStyle getStyleForCustomContainer(
            ContainmentItem container, boolean isSingleLine) {
        if (container.getCustomBackgroundStyle() == BackgroundStyle.CARD) {
            int topMargin = container.getCustomTopMargin();
            if (topMargin == DEFAULT_MARGIN) topMargin = mDefaultContainerVerticalMargin;

            int bottomMargin = container.getCustomBottomMargin();
            if (bottomMargin == DEFAULT_MARGIN) {
                bottomMargin = mDefaultContainerVerticalMargin + mSectionBottomAdditionalMargin;
            }

            int horizontalMargin = container.getCustomHorizontalMargin();
            if (horizontalMargin == DEFAULT_MARGIN) {
                horizontalMargin = mDefaultMargin;
            }

            int backgroundColor = container.getCustomBackgroundColor();
            if (backgroundColor == DEFAULT_COLOR) backgroundColor = mDefaultBackgroundColor;

            return new ContainerStyle.Builder()
                    .setTopRadius(mDefaultRadius)
                    .setBottomRadius(mDefaultRadius)
                    .setTopMargin(topMargin)
                    .setBottomMargin(bottomMargin)
                    .setVerticalPadding(isSingleLine ? mDefaultPadding : mMultiLineVerticalPadding)
                    .setHorizontalMargin(horizontalMargin)
                    .setBackgroundColor(backgroundColor)
                    .build();
        }

        return ContainerStyle.EMPTY;
    }

    /**
     * Creates a default {@link ContainerStyle} for a standard item. The style is determined by
     * whether the item is at the top or bottom of a styling section.
     *
     * @param isTop Whether the item is at the top of a section.
     * @param isBottom Whether the item is at the bottom of a section.
     * @param isSingleLine Whether the item is single line.
     * @return The {@link ContainerStyle} for the item.
     */
    public ContainerStyle createBuilderWithDefaultStyle(
            boolean isTop, boolean isBottom, boolean isSingleLine) {
        float topRadius = mDefaultRadius;
        float bottomRadius = mDefaultRadius;
        int bottomMargin = mDefaultContainerVerticalMargin;
        int verticalPadding = isSingleLine ? mDefaultPadding : mMultiLineVerticalPadding;

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

        return new ContainerStyle.Builder()
                .setTopRadius(topRadius)
                .setBottomRadius(bottomRadius)
                .setTopMargin(mDefaultContainerVerticalMargin)
                .setBottomMargin(bottomMargin)
                .setHorizontalMargin(mDefaultMargin)
                .setVerticalPadding(verticalPadding)
                .setBackgroundColor(mDefaultBackgroundColor)
                .build();
    }
}
