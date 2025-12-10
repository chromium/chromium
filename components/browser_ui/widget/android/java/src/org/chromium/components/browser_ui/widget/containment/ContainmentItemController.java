// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import static org.chromium.components.browser_ui.styles.SemanticColorUtils.getSettingsContainerBackgroundColor;
import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_COLOR;
import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_VALUE;

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
 * <p>This controller generates {@link ContainerStyle} objects for {@link Preference} items and
 * generic {@link View}s. It determines styles based on an item's position within a styling section
 * and its custom styling properties. Styling is composed by calling a series of helper methods:
 *
 * <ul>
 *   <li>{@link #addStandardStyling}: Applies standard margins and padding.
 *   <li>{@link #addPositionBasedStyling}: Applies corner radii and margins based on the item's
 *       position in a section.
 *   <li>{@link #addCustomStyling}: Applies custom styles from {@link ContainmentItem}.
 * </ul>
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
     * Determines the style for a preference based on its position in the list and its custom
     * styling properties.
     *
     * <p>This method evaluates the preference's position relative to its neighbors to determine if
     * it's at the top, middle, bottom, or a standalone item in a section. It also checks if the
     * preference implements {@link ContainmentItem} to apply custom styles. The final {@link
     * ContainerStyle} is composed by calling helper methods to add position-based and custom
     * styling.
     *
     * @param visiblePreferences The list of all visible preferences.
     * @param position The position of the current preference in the list.
     * @return The {@link ContainerStyle} for the preference.
     */
    private @NonNull ContainerStyle getPreferenceStyleForPosition(
            ArrayList<Preference> visiblePreferences, int position) {
        Preference currentPref = visiblePreferences.get(position);

        if (currentPref instanceof PreferenceCategory) {
            return new ContainerStyle.Builder()
                    .setBottomMargin(mDefaultMargin)
                    .setHorizontalMargin(mDefaultMargin)
                    .setBackgroundColor(TRANSPARENT_BACKGROUND_COLOR)
                    .build();
        }
        if (currentPref instanceof ContainmentItem customStyledPreference) {
            if (customStyledPreference.getCustomBackgroundStyle() == BackgroundStyle.NONE) {
                return ContainerStyle.EMPTY;
            }
        }

        ContainerStyle.Builder containerStyleBuilder = new ContainerStyle.Builder();

        // Evaluate position of preference
        Preference prefAbove = (position > 0) ? visiblePreferences.get(position - 1) : null;
        Preference prefBelow =
                (position < visiblePreferences.size() - 1)
                        ? visiblePreferences.get(position + 1)
                        : null;
        boolean isTop = (prefAbove == null) || isCustomStyledPreference(prefAbove);
        boolean isBottom = (prefBelow == null) || isCustomStyledPreference(prefBelow);

        addStandardStyling(containerStyleBuilder, currentPref.getSummary() == null);
        if (currentPref instanceof ContainmentItem customStyledPreference) {
            if (customStyledPreference.getCustomBackgroundStyle() == BackgroundStyle.CARD) {
                isTop = true;
                isBottom = true;
            }
            addCustomStyling(containerStyleBuilder, customStyledPreference);
        }
        addPositionBasedStyling(containerStyleBuilder, isTop, isBottom);
        return containerStyleBuilder.build();
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
     * Determines the style for a view based on its position in a list. This method only applies
     * styling to views that implement the {@link ContainmentItem} interface.
     *
     * <p>The method determines the view's position in a styling section (e.g., top, bottom) based
     * on its {@link BackgroundStyle} and its neighbors. It then composes the final {@link
     * ContainerStyle} by calling helper methods for custom and position-based styling.
     *
     * @param views The list of all views to be styled.
     * @param position The position of the current view in the list.
     * @return The {@link ContainerStyle} for the view, or {@link ContainerStyle#EMPTY} if the view
     *     is not a {@link ContainmentItem}.
     */
    private ContainerStyle getViewStyleForPosition(List<View> views, int position) {
        View view = views.get(position);

        if (!(view instanceof ContainmentItem customStyledView)
                || customStyledView.getCustomBackgroundStyle() == BackgroundStyle.NONE) {
            return ContainerStyle.EMPTY;
        }

        boolean isTop = position == 0 || isCustomStyledView(views.get(position - 1));
        boolean isBottom =
                position == views.size() - 1 || isCustomStyledView(views.get(position + 1));

        ContainerStyle.Builder containerStyleBuilder = new ContainerStyle.Builder();
        addStandardStyling(containerStyleBuilder, /* isSingleLine= */ true);
        if (customStyledView.getCustomBackgroundStyle() == BackgroundStyle.CARD) {
            isTop = true;
            isBottom = true;
        }
        addCustomStyling(containerStyleBuilder, customStyledView);
        addPositionBasedStyling(containerStyleBuilder, isTop, isBottom);
        return containerStyleBuilder.build();
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
     * Creates a {@link ContainerStyle.Builder} and applies standard, custom, and position-based
     * styling.
     *
     * @param isTop Whether the item is at the top of a section.
     * @param isBottom Whether the item is at the bottom of a section.
     * @param isSingleLine Whether the item is single-line, affecting vertical padding.
     * @return A {@link ContainerStyle.Builder} with all styles applied.
     */
    public ContainerStyle.Builder createStandardBuilder(
            boolean isTop, boolean isBottom, boolean isSingleLine) {
        ContainerStyle.Builder containerStyleBuilder = new ContainerStyle.Builder();
        addStandardStyling(containerStyleBuilder, isSingleLine);
        addPositionBasedStyling(containerStyleBuilder, isTop, isBottom);
        return containerStyleBuilder;
    }

    /**
     * Applies standard margins, padding, and the default background color to a {@link
     * ContainerStyle.Builder}.
     *
     * @param containerStyleBuilder The {@link ContainerStyle.Builder} to apply the styles to.
     * @param isSingleLine Whether the item is single-line, affecting vertical padding.
     */
    private void addStandardStyling(
            ContainerStyle.Builder containerStyleBuilder, boolean isSingleLine) {
        containerStyleBuilder
                .setTopMargin(mDefaultContainerVerticalMargin)
                .setHorizontalMargin(mDefaultMargin)
                .setVerticalPadding(isSingleLine ? mDefaultPadding : mMultiLineVerticalPadding)
                .setBackgroundColor(mDefaultBackgroundColor);
    }

    /**
     * Applies custom styling from a {@link ContainmentItem} to a {@link ContainerStyle.Builder}.
     * This includes background color and minimum height. If the item does not specify custom
     * values, defaults are used.
     *
     * @param containerStyleBuilder The {@link ContainerStyle.Builder} to apply the styles to.
     * @param container The {@link ContainmentItem} from which to source custom styles.
     */
    public void addCustomStyling(
            ContainerStyle.Builder containerStyleBuilder, ContainmentItem container) {

        int backgroundColor = container.getCustomBackgroundColor();
        if (backgroundColor == DEFAULT_COLOR) backgroundColor = mDefaultBackgroundColor;

        containerStyleBuilder.setBackgroundColor(backgroundColor);

        int minHeight = container.getCustomMinHeight();
        if (minHeight != DEFAULT_VALUE) {
            // Only set the default height if a custom value was provided
            containerStyleBuilder.setMinHeight(minHeight);
        }
    }

    /**
     * Applies corner radii and bottom margin to a {@link ContainerStyle.Builder} based on the
     * item's position within a styling section (top, bottom, middle, or standalone).
     *
     * @param containerStyleBuilder The {@link ContainerStyle.Builder} to apply the styles to.
     * @param isTop Whether the item is at the top of a section.
     * @param isBottom Whether the item is at the bottom of a section.
     */
    public void addPositionBasedStyling(
            ContainerStyle.Builder containerStyleBuilder, boolean isTop, boolean isBottom) {
        float topRadius = mDefaultRadius;
        float bottomRadius = mDefaultRadius;
        int bottomMargin = mDefaultContainerVerticalMargin;

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

        containerStyleBuilder
                .setBottomMargin(bottomMargin)
                .setTopRadius(topRadius)
                .setBottomRadius(bottomRadius);
    }
}
