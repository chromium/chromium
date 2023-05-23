// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
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
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
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
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.util.ColorUtils;

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
    /**
     * Holder of metadata about a view's current state w.r.t. a suggestion's visual properties.
     * This allows us to avoid calling setters when the current state of the view is already
     * correct.
     */
    private static class BaseSuggestionViewMetadata {
        @Nullable
        public Drawable.ConstantState backgroundConstantState;
    }

    /** Drawable ConstantState used to expedite creation of Focus ripples. */
    private static Drawable.ConstantState sFocusableDrawableState;
    private static @BrandedColorScheme int sFocusableDrawableStateTheme;
    private static boolean sFocusableDrawableStateInNightMode;
    private final ViewBinder<PropertyModel, T, PropertyKey> mContentBinder;

    private static boolean sDimensionsInitialized;
    private static int sIconWidthPx;
    private static int sPaddingSmallIcon;
    private static int sPaddingStartLargeIcon;
    private static int sPaddingEndLargeIcon;
    private static int sEdgeSize;
    private static int sEdgeSizeLargeIcon;
    private static int sSideSpacing;

    public BaseSuggestionViewBinder(ViewBinder<PropertyModel, T, PropertyKey> contentBinder) {
        mContentBinder = contentBinder;
    }

    @Override
    public void bind(PropertyModel model, BaseSuggestionView<T> view, PropertyKey propertyKey) {
        if (!sDimensionsInitialized) {
            initializeDimensions(view.getContext());
            sDimensionsInitialized = true;
        }

        mContentBinder.bind(model, view.getContentView(), propertyKey);
        ActionChipsBinder.bind(model, view.getActionChipsView(), propertyKey);

        if (BaseSuggestionViewProperties.ICON == propertyKey) {
            updateSuggestionIcon(model, view);
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            updateColorScheme(model, view);
        } else if (DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED == propertyKey
                || DropdownCommonProperties.BG_TOP_CORNER_ROUNDED == propertyKey) {
            roundSuggestionViewCorners(model, view);
        } else if (DropdownCommonProperties.TOP_MARGIN == propertyKey
                || DropdownCommonProperties.BOTTOM_MARGIN == propertyKey) {
            updateMargin(model, view);
        } else if (BaseSuggestionViewProperties.ACTION_BUTTONS == propertyKey) {
            bindActionButtons(model, view, model.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        } else if (BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION == propertyKey) {
            view.setOnFocusViaSelectionListener(
                    model.get(BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION));
        } else if (BaseSuggestionViewProperties.ON_CLICK == propertyKey) {
            Runnable listener = model.get(BaseSuggestionViewProperties.ON_CLICK);
            if (listener == null) {
                view.setOnClickListener(null);
            } else {
                view.setOnClickListener(v -> listener.run());
            }
        } else if (BaseSuggestionViewProperties.ON_LONG_CLICK == propertyKey) {
            Runnable listener = model.get(BaseSuggestionViewProperties.ON_LONG_CLICK);
            if (listener == null) {
                view.setOnLongClickListener(null);
            } else {
                view.setOnLongClickListener(v -> {
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
        final List<ImageView> actionViews = view.getActionButtons();
        for (int index = 0; index < actionCount; index++) {
            final ImageView actionView = actionViews.get(index);
            final Action action = actions.get(index);
            actionView.setOnClickListener(v -> action.callback.run());
            actionView.setContentDescription(action.accessibilityDescription);
            applySelectableBackground(model, actionView);
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
        maybeResetCachedFocusableDrawableState(model, view);
        updateSuggestionIcon(model, view);
        applySelectableBackground(model, view);

        final List<Action> actions = model.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        // Setting ACTION_BUTTONS and updating actionViews can happen later. Appropriate color
        // scheme will be applied then.
        if (actions == null) return;

        final List<ImageView> actionViews = view.getActionButtons();
        for (int index = 0; index < actionViews.size(); index++) {
            ImageView actionView = actionViews.get(index);
            applySelectableBackground(model, actionView);
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
            rciv.setLayoutParams(new SuggestionLayout.LayoutParams(sIconWidthPx,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    SuggestionLayout.LayoutParams.SuggestionViewType.DECORATION));

            int paddingStart = sds.isLarge ? sPaddingStartLargeIcon : sPaddingSmallIcon;
            int paddingEnd = sds.isLarge ? sPaddingEndLargeIcon : sPaddingSmallIcon;
            int edgeSize = sds.isLarge ? sEdgeSizeLargeIcon : sEdgeSize;
            rciv.setPadding(paddingStart, 0, paddingEnd, 0);
            rciv.setMinimumHeight(edgeSize);
            rciv.setClipToOutline(sds.useRoundedCorners);
        }

        updateIcon(rciv, sds, ChromeColors.getSecondaryIconTintRes(isIncognito(model)));
    }

    /**
     * Access the BaseSuggestionViewMetadata for the given view, creating and attaching a new one
     * if none is currently associated. Returns an unattached metadata if {@link
     * OmniboxFeatures#shouldCacheSuggestionResources} returns false.
     */
    private static @NonNull BaseSuggestionViewMetadata ensureViewMetadata(View view) {
        if (!OmniboxFeatures.shouldCacheSuggestionResources()) {
            return new BaseSuggestionViewMetadata();
        }

        BaseSuggestionViewMetadata metadata =
                (BaseSuggestionViewMetadata) view.getTag(R.id.base_suggestion_view_metadata_key);
        if (metadata == null) {
            metadata = new BaseSuggestionViewMetadata();
            view.setTag(R.id.base_suggestion_view_metadata_key, metadata);
        }
        return metadata;
    }

    /**
     * Applies selectable drawable from cache (where possible) or resources (otherwise).
     *
     * The method internally stores the ConstantState for the drawable to be returned to
     * accelerate creation of subsequent objects.
     *
     * @param model A property model to look up relevant properties.
     * @param view A view that receives background.
     */
    public static void applySelectableBackground(PropertyModel model, View view) {
        // Use a throwaway metadata object if caching is off to simplify branching; the performance
        // difference will still manifest because it's not persisted.
        BaseSuggestionViewMetadata metadata = ensureViewMetadata(view);

        if (sFocusableDrawableState != null) {
            if (sFocusableDrawableState == metadata.backgroundConstantState) return;
            view.setBackground(sFocusableDrawableState.newDrawable());
            metadata.backgroundConstantState = sFocusableDrawableState;
            return;
        }

        // Background color to be used for suggestions
        var ctx = view.getContext();
        var background = new ColorDrawable(getSuggestionBackgroundColor(model, view.getContext()));
        // Ripple effect to use when the user interacts with the suggestion.
        var ripple = OmniboxResourceProvider.resolveAttributeToDrawable(ctx,
                model.get(SuggestionCommonProperties.COLOR_SCHEME),
                R.attr.selectableItemBackground);

        var layer = new LayerDrawable(new Drawable[] {background, ripple});

        // Cache the drawable state for faster retrieval.
        // See go/omnibox:drawables for more details.
        sFocusableDrawableState = layer.getConstantState();
        metadata.backgroundConstantState = sFocusableDrawableState;
        view.setBackground(layer);
    }

    /**
     * Retrieve the background color to be applied to suggestion.
     *
     * @param model A property model to look up relevant properties.
     * @param ctx Context used to retrieve appropriate color value.
     * @return @ColorInt value representing the color to be applied.
     */
    public static @ColorInt int getSuggestionBackgroundColor(PropertyModel model, Context ctx) {
        return isIncognito(model)
                ? ctx.getColor(R.color.omnibox_suggestion_bg_incognito)
                : ChromeColors.getSurfaceColor(ctx, R.dimen.omnibox_suggestion_bg_elevation);
    }

    /**
     * Checks whether cached FocusableDrawableState should be reset.
     *
     * TODO(ender): Relocate this to appropriate OmniboxResourceManager class.
     *
     * @param model The model to supply app-driven changes.
     * @param view The view to supply additional information, such as UI configuration.
     */
    @VisibleForTesting
    public static void maybeResetCachedFocusableDrawableState(PropertyModel model, View view) {
        // The color theme has changed, or the user opened Incognito window.
        // Reset the cached drawable state to prevent using old colors.
        var theme = model.get(SuggestionCommonProperties.COLOR_SCHEME);
        // The theme change may also originate from the system.
        // Be sure we respond to these changes as well.
        // This aspect should only be relevant when the theme is APP_DEFAULT.
        var isInNightMode = ColorUtils.inNightMode(view.getContext());
        if (theme != sFocusableDrawableStateTheme
                || isInNightMode != sFocusableDrawableStateInNightMode) {
            sFocusableDrawableState = null;
            sFocusableDrawableStateTheme = theme;
            sFocusableDrawableStateInNightMode = isInNightMode;
        }
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
     * Update the margin for the view.
     *
     * @param model A property model to look up relevant properties.
     * @param view A view that need to be updated.
     */
    public static void updateMargin(PropertyModel model, View view) {
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (layoutParams == null) {
            layoutParams =
                    new MarginLayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        }

        if (layoutParams instanceof MarginLayoutParams) {
            int topSpacing = model.get(DropdownCommonProperties.TOP_MARGIN);
            int bottomSpacing = model.get(DropdownCommonProperties.BOTTOM_MARGIN);
            ((MarginLayoutParams) layoutParams)
                    .setMargins(sSideSpacing, topSpacing, sSideSpacing, bottomSpacing);
        }
        view.setLayoutParams(layoutParams);
    }

    /**
     * Round top/bottom suggestion view corners to mark suggestions that begin or end section -- or
     * are standalone suggestions.
     *
     * The rounding mechanism utilizes OutlineProviders to guarantee that focus and selection won't
     * leak outside of the rounded edges.
     *
     * @param model A property model, defining which corners (specifically: corners along which
     *         edge) should be rounded,
     * @param view The view that should receive rounding.
     */
    private static void roundSuggestionViewCorners(PropertyModel model, View view) {
        var roundTopEdge = model.get(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED);
        var roundBottomEdge = model.get(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED);

        if (!roundTopEdge && !roundBottomEdge) {
            // Note: Suggestion views are re-used. Make sure we don't carry over rounding from
            // previous model.
            view.setClipToOutline(false);
            return;
        }

        // TODO(crbug.com/1418077): This should be part of BaseSuggestionView.
        // Move this once we reconcile Pedals with Base.
        var outlineProvider = view.getOutlineProvider();
        if (!(outlineProvider instanceof RoundedCornerOutlineProvider)) {
            outlineProvider =
                    new RoundedCornerOutlineProvider(view.getResources().getDimensionPixelSize(
                            R.dimen.omnibox_suggestion_bg_round_corner_radius));
            view.setOutlineProvider(outlineProvider);
        }
        RoundedCornerOutlineProvider roundedCornerOutlineProvider =
                (RoundedCornerOutlineProvider) outlineProvider;
        roundedCornerOutlineProvider.setRoundingEdges(true, roundTopEdge, true, roundBottomEdge);
        view.setClipToOutline(true);
    }

    @VisibleForTesting
    static void initializeDimensions(Context context) {
        boolean showModernizeVisualUpdate =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(context);
        Resources resources = context.getResources();
        sIconWidthPx = resources.getDimensionPixelSize(showModernizeVisualUpdate
                        ? R.dimen.omnibox_suggestion_icon_area_size_modern
                        : R.dimen.omnibox_suggestion_icon_area_size);

        sPaddingSmallIcon = showModernizeVisualUpdate
                ? OmniboxResourceProvider.getIconStartPadding(context)
                : resources.getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_24dp_icon_margin_start);
        sPaddingStartLargeIcon = OmniboxResourceProvider.getLargeIconStartPadding(context);
        sPaddingEndLargeIcon = OmniboxResourceProvider.getLargeIconEndPadding(context);
        sEdgeSize = resources.getDimensionPixelSize(R.dimen.omnibox_suggestion_24dp_icon_size);
        sEdgeSizeLargeIcon =
                resources.getDimensionPixelSize(R.dimen.omnibox_suggestion_36dp_icon_size);
        sSideSpacing = OmniboxResourceProvider.getSideSpacing(context);
    }

    /** @return Cached ConstantState for testing. */
    @VisibleForTesting
    public static Drawable.ConstantState getFocusableDrawableStateForTesting() {
        return sFocusableDrawableState;
    }
}
