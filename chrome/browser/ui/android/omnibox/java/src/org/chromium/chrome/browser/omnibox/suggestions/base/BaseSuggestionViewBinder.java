// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

import java.util.List;

/**
 * Binds base suggestion view properties.
 *
 * This binder should be used by all suggestions that also utilize BaseSuggestionView<T> to
 * construct the view, and manages shared suggestion properties (such as decorations or theme).
 *
 * @param <T> The inner content view type being updated.
 */
public final class BaseSuggestionViewBinder<T extends View>
        implements ViewBinder<PropertyModel, BaseSuggestionView<T>, PropertyKey> {
    private final ViewBinder<PropertyModel, T, PropertyKey> mContentBinder;

    public BaseSuggestionViewBinder(ViewBinder<PropertyModel, T, PropertyKey> contentBinder) {
        mContentBinder = contentBinder;
    }

    @Override
    public void bind(PropertyModel model, BaseSuggestionView<T> view, PropertyKey propertyKey) {
        mContentBinder.bind(model, view.getContentView(), propertyKey);

        if (BaseSuggestionViewProperties.ICON == propertyKey) {
            updateSuggestionIcon(model, view);
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (BaseSuggestionViewProperties.DENSITY == propertyKey) {
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            updateColorScheme(model, view);
        } else if (DropdownCommonProperties.BG_TOP_CORNER_ROUNDED == propertyKey) {
            updateBackground(model, view);
        } else if (DropdownCommonProperties.TOP_MARGIN == propertyKey) {
            updateMargin(model, view);
        } else if (BaseSuggestionViewProperties.ACTIONS == propertyKey) {
            bindActionButtons(model, view, model.get(BaseSuggestionViewProperties.ACTIONS));
        } else if (BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION == propertyKey) {
            view.setOnFocusViaSelectionListener(
                    model.get(BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION));
        } else if (BaseSuggestionViewProperties.ON_CLICK == propertyKey) {
            Runnable listener = model.get(BaseSuggestionViewProperties.ON_CLICK);
            if (listener == null) {
                view.getDecoratedSuggestionView().setOnClickListener(null);
            } else {
                view.getDecoratedSuggestionView().setOnClickListener(v -> listener.run());
            }
        } else if (BaseSuggestionViewProperties.ON_LONG_CLICK == propertyKey) {
            Runnable listener = model.get(BaseSuggestionViewProperties.ON_LONG_CLICK);
            if (listener == null) {
                view.getDecoratedSuggestionView().setOnLongClickListener(null);
            } else {
                view.getDecoratedSuggestionView().setOnLongClickListener(v -> {
                    listener.run();
                    return true;
                });
            }
        }
    }

    /** Bind Action Icons for the suggestion view. */
    private static <T extends View> void bindActionButtons(
            PropertyModel model, BaseSuggestionView<T> view, List<Action> actions) {
        final int actionCount = actions != null ? actions.size() : 0;
        view.setActionButtonsCount(actionCount);

        // Drawable retrieved once here (expensive) and will be copied multiple times (cheap).
        Drawable backgroundDrawable = getSelectableBackgroundDrawable(view, model);
        final List<ImageView> actionViews = view.getActionButtons();
        for (int index = 0; index < actionCount; index++) {
            final ImageView actionView = actionViews.get(index);
            final Action action = actions.get(index);
            actionView.setOnClickListener(v -> action.callback.run());
            actionView.setContentDescription(action.accessibilityDescription);
            actionView.setBackground(copyDrawable(backgroundDrawable));
            updateIcon(actionView, action.icon,
                    ChromeColors.getPrimaryIconTintRes(isIncognito(model)));

            actionView.setAccessibilityDelegate(new AccessibilityDelegate() {
                @Override
                public void onInitializeAccessibilityNodeInfo(
                        View host, AccessibilityNodeInfo info) {
                    super.onInitializeAccessibilityNodeInfo(host, info);
                    info.addAction(AccessibilityAction.ACTION_CLICK);
                }

                @Override
                public boolean performAccessibilityAction(
                        View host, int accessibilityAction, Bundle arguments) {
                    if (accessibilityAction == AccessibilityNodeInfo.ACTION_CLICK
                            && action.onClickAnnouncement != null) {
                        actionView.announceForAccessibility(action.onClickAnnouncement);
                    }
                    return super.performAccessibilityAction(host, accessibilityAction, arguments);
                }
            });
        }
    }

    /** Update visual theme to reflect dark mode UI theme update. */
    private static <T extends View> void updateColorScheme(
            PropertyModel model, BaseSuggestionView<T> view) {
        updateSuggestionIcon(model, view);
        Drawable backgroundDrawable = getSelectableBackgroundDrawable(view, model);
        view.getDecoratedSuggestionView().setBackground(backgroundDrawable);

        final List<Action> actions = model.get(BaseSuggestionViewProperties.ACTIONS);
        // Setting ACTIONS and updating actionViews can happen later. Appropriate color scheme will
        // be applied then.
        if (actions == null) return;

        final List<ImageView> actionViews = view.getActionButtons();
        for (int index = 0; index < actionViews.size(); index++) {
            ImageView actionView = actionViews.get(index);
            actionView.setBackground(copyDrawable(backgroundDrawable));
            updateIcon(actionView, actions.get(index).icon,
                    ChromeColors.getPrimaryIconTintRes(isIncognito(model)));
        }
    }

    /** @return Whether the current {@link BrandedColorScheme} is INCOGNITO. */
    private static boolean isIncognito(PropertyModel model) {
        return model.get(SuggestionCommonProperties.COLOR_SCHEME) == BrandedColorScheme.INCOGNITO;
    }

    /** Update attributes of decorated suggestion icon. */
    private static <T extends View> void updateSuggestionIcon(
            PropertyModel model, BaseSuggestionView<T> baseView) {
        final ImageView rciv = baseView.getSuggestionImageView();
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ICON);

        if (sds != null) {
            final Resources res = rciv.getContext().getResources();
            boolean showModernizeVisualUpdate =
                    OmniboxFeatures.shouldShowModernizeVisualUpdate(rciv.getContext());
            int iconWidthPx = res.getDimensionPixelSize(showModernizeVisualUpdate
                            ? R.dimen.omnibox_suggestion_icon_area_size_modern
                            : R.dimen.omnibox_suggestion_icon_area_size);

            rciv.setLayoutParams(new SimpleHorizontalLayoutView.LayoutParams(
                    iconWidthPx, ViewGroup.LayoutParams.WRAP_CONTENT));

            final int paddingStart = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_margin_start
                            : showModernizeVisualUpdate
                            ? R.dimen.omnibox_suggestion_24dp_icon_margin_start_modern
                            : R.dimen.omnibox_suggestion_24dp_icon_margin_start);
            final int paddingEnd = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_margin_end
                            : R.dimen.omnibox_suggestion_24dp_icon_margin_end);
            final int edgeSize = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_size
                            : R.dimen.omnibox_suggestion_24dp_icon_size);

            rciv.setPadding(paddingStart, 0, paddingEnd, 0);
            rciv.setMinimumHeight(edgeSize);
            rciv.setClipToOutline(sds.useRoundedCorners);
        }

        updateIcon(rciv, sds, ChromeColors.getSecondaryIconTintRes(isIncognito(model)));
    }

    /**
     * Update content view padding.
     * This is required only to adjust the leading padding for undecorated suggestions.
     * TODO(crbug.com/1019937): remove after suggestion favicons are launched.
     */
    private static <T extends View> void updateContentViewPadding(
            PropertyModel model, DecoratedSuggestionView<T> view) {
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ICON);
        final int startSpace = sds == null ? view.getResources().getDimensionPixelSize(
                                       R.dimen.omnibox_suggestion_start_offset_without_icon)
                                           : 0;

        // TODO(ender): Drop this view and expand the last icon size by 8dp to ensure it remains
        // centered with the omnibox "Clear" button.
        final int endSpace = view.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);
        view.setPaddingRelative(startSpace, 0, endSpace, 0);

        // Compact suggestion handling: apply additional padding to the suggestion content.
        final @BaseSuggestionViewProperties.Density int density =
                model.get(BaseSuggestionViewProperties.DENSITY);

        int minimumHeightRes;
        int verticalPadRes;
        switch (density) {
            case BaseSuggestionViewProperties.Density.COMPACT:
                verticalPadRes = R.dimen.omnibox_suggestion_compact_padding;
                minimumHeightRes = R.dimen.omnibox_suggestion_compact_height;
                break;
            case BaseSuggestionViewProperties.Density.DEFAULT:
            default:
                verticalPadRes = R.dimen.omnibox_suggestion_semicompact_padding;
                minimumHeightRes = R.dimen.omnibox_suggestion_semicompact_height;
                break;
        }
        final int verticalPad = view.getResources().getDimensionPixelSize(verticalPadRes);
        view.getContentView().setPaddingRelative(0, verticalPad, 0, verticalPad);

        final int minimumHeight = view.getResources().getDimensionPixelSize(minimumHeightRes);
        view.getContentView().setMinimumHeight(minimumHeight);
    }

    /**
     * Retrieves selecatable background drawable from resources. If possible prefer
     * {@link #copyDrawable(Drawable)} over this operation, as it offers an order of magnitude
     * better performance in incognito.
     * The drawable should be used only once, all other uses should make a copy.
     *
     * @param view A view that provides context.
     * @param model A property model to look up relevant properties.
     * @return A selectable background drawable.
     */
    public static Drawable getSelectableBackgroundDrawable(View view, PropertyModel model) {
        return OmniboxResourceProvider.resolveAttributeToDrawable(view.getContext(),
                model.get(SuggestionCommonProperties.COLOR_SCHEME),
                R.attr.selectableItemBackground);
    }

    /**
     * Creates a copy of the drawable. The drawable should be used only once, all other uses should
     * make a copy.
     *
     * @param original Original drawable to be copied.
     * @return Copied drawable.
     */
    private static Drawable copyDrawable(Drawable original) {
        return original.getConstantState().newDrawable();
    }

    /** Update image view using supplied drawable state object. */
    private static void updateIcon(
            ImageView view, SuggestionDrawableState sds, @ColorRes int tintRes) {
        view.setVisibility(sds == null ? View.GONE : View.VISIBLE);
        if (sds == null) {
            // Release any drawable that is still attached to this view to reclaim memory.
            view.setImageDrawable(null);
            return;
        }

        ColorStateList tint = null;
        if (sds.allowTint) {
            tint = AppCompatResources.getColorStateList(view.getContext(), tintRes);
        }

        view.setImageDrawable(sds.drawable);
        ImageViewCompat.setImageTintList(view, tint);
    }

    /**
     * Update the background for the view, also add the margin for the view.
     *
     * @param model A property model to look up relevant properties.
     * @param view A view that need to be updated.
     */
    public static void updateBackground(PropertyModel model, View view) {
        view.setBackground(getBackgroundDrawable(model, view));
    }

    /**
     * Update the margin for the view.
     *
     * @param model A property model to look up relevant properties.
     * @param view A view that need to be updated.
     */
    public static void updateMargin(PropertyModel model, View view) {
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (layoutParams == null) {
            layoutParams =
                    new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        }

        if (layoutParams instanceof MarginLayoutParams) {
            int topSpacing = model.get(DropdownCommonProperties.TOP_MARGIN);
            int bottomSpacing = model.get(DropdownCommonProperties.BOTTOM_MARGIN);
            int sideSpacing = view.getContext().getResources().getDimensionPixelOffset(
                    R.dimen.omnibox_suggestion_side_spacing);
            ((MarginLayoutParams) layoutParams)
                    .setMargins(sideSpacing, topSpacing, sideSpacing, bottomSpacing);
        }
        view.setLayoutParams(layoutParams);
    }

    /**
     * Retrieves background drawable for the view.
     *
     * @param model A property model to look up relevant properties.
     * @param view A view that provides context.
     * @return The suggestion background drawable.
     */
    private static Drawable getBackgroundDrawable(PropertyModel model, View view) {
        final Resources resources = view.getContext().getResources();
        int roundedRadius =
                resources.getDimensionPixelSize(R.dimen.omnibox_suggestion_bg_round_corner_radius);
        int rectangleRadius = resources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_bg_rectangle_corner_radius);

        int topRadii = model.get(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED) ? roundedRadius
                                                                                 : rectangleRadius;
        int bottomRadii = model.get(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED)
                ? roundedRadius
                : rectangleRadius;

        GradientDrawable backgroundGradient = new GradientDrawable();
        backgroundGradient.setShape(GradientDrawable.RECTANGLE);

        backgroundGradient.setCornerRadii(new float[] {topRadii, topRadii, topRadii, topRadii,
                bottomRadii, bottomRadii, bottomRadii, bottomRadii});
        backgroundGradient.setColor(getBackgroundDrawableColor(isIncognito(model), view));

        return backgroundGradient;
    }

    /**
     * Retrieves color for background gradient based on identifying incognito mode.
     *
     * @param isIncognito whether the view is in incognito mode.
     * @param view A view that provides context.
     * @return The color for suggestion background drawable.

     */
    static @ColorInt int getBackgroundDrawableColor(boolean isIncognito, View view) {
        return isIncognito ? view.getContext().getColor(R.color.omnibox_suggestion_bg_incognito)
                           : ChromeColors.getSurfaceColor(
                                   view.getContext(), R.dimen.omnibox_suggestion_bg_elevation);
    }
}
