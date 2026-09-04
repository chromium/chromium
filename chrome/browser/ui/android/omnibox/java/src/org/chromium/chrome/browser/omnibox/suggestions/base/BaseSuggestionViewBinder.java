// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.TooltipCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.PositionalMode;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.RoundSides;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.util.ColorUtils;

import java.util.List;

/**
 * Binds base suggestion view properties.
 *
 * <p>This binder should be used by all suggestions that also utilize BaseSuggestionView<T> to
 * construct the view, and manages shared suggestion properties (such as decorations or theme).
 *
 * @param <T> The inner content view type being updated.
 */
@NullMarked
public abstract class BaseSuggestionViewBinder<T extends View>
        implements ViewBinder<PropertyModel, BaseSuggestionView<T>, PropertyKey> {
    /**
     * Holder of metadata about a view's current state w.r.t. a suggestion's visual properties. This
     * allows us to avoid calling setters when the current state of the view is already correct.
     */
    private static class BaseSuggestionViewMetadata {
        public @Nullable ConstantState backgroundConstantState;
    }

    /** Drawable ConstantState used to expedite creation of Focus ripples. */
    @VisibleForTesting static @Nullable ConstantState sFocusableDrawableState;

    private static @BrandedColorScheme int sFocusableDrawableStateTheme;
    private static boolean sFocusableDrawableStateInNightMode;

    protected static OmniboxResourceProvider getResourceProvider(PropertyModel model) {
        OmniboxResourceProvider provider = model.get(SuggestionCommonProperties.RESOURCE_PROVIDER);
        return assumeNonNull(provider);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public void bind(PropertyModel model, BaseSuggestionView<T> view, PropertyKey propertyKey) {
        view.setSuggestionDimensions(
                getResourceProvider(model).getSuggestionDecorationIconSizeWidth(),
                getResourceProvider(model).getSuggestionContentHeight(),
                getResourceProvider(model).getSuggestionCompactContentHeight(),
                getResourceProvider(model).getSuggestionContentVerticalPadding());

        bindContent(model, view.contentView, propertyKey);
        ActionChipsBinder.bind(model, view.actionChipsView, propertyKey);

        if (BaseSuggestionViewProperties.ACTION_BUTTONS == propertyKey) {
            bindActionButtons(model, view, model.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        } else if (BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING == propertyKey) {
            view.setActionChipLeadInSpacing(
                    model.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING));
        } else if (SuggestionCommonProperties.APPLY_SIDE_SPACING == propertyKey) {
            view.applySideSpacing(
                    model.get(SuggestionCommonProperties.APPLY_SIDE_SPACING),
                    getResourceProvider(model).getSideSpacing());
        } else if (SuggestionCommonProperties.BG_POSITIONAL_MODE == propertyKey
                || SuggestionCommonProperties.BG_ROUND_SIDES == propertyKey) {
            updateRounding(model, view);
        } else if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            updateColorScheme(model, view);
        } else if (BaseSuggestionViewProperties.ICON == propertyKey) {
            updateSuggestionIcon(model, view);
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (BaseSuggestionViewProperties.ON_ACTIVATE == propertyKey) {
            view.setOnActivateListener(model.get(BaseSuggestionViewProperties.ON_ACTIVATE));
        } else if (BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION == propertyKey) {
            view.setOnFocusViaSelectionListener(
                    model.get(BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION));
        } else if (BaseSuggestionViewProperties.ON_LONG_CLICK == propertyKey) {
            Runnable listener = model.get(BaseSuggestionViewProperties.ON_LONG_CLICK);
            if (listener == null) {
                view.setOnLongClickListener(null);
            } else {
                view.setOnLongClickListener(
                        v -> {
                            listener.run();
                            return true;
                        });
            }
        } else if (BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT == propertyKey) {
            Callback<Long> listener = model.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
            if (listener == null) {
                view.setOnTouchListener(null);
            } else {
                view.setOnTouchListener(
                        (v, event) -> {
                            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                                listener.onResult(event.getEventTime());
                            }
                            return false;
                        });
            }
        } else if (BaseSuggestionViewProperties.SHOW_DECORATION == propertyKey) {
            view.setShowDecorationIcon(model.get(BaseSuggestionViewProperties.SHOW_DECORATION));
        } else if (BaseSuggestionViewProperties.TOP_PADDING == propertyKey) {
            view.setPaddingRelative(
                    view.getPaddingStart(),
                    model.get(BaseSuggestionViewProperties.TOP_PADDING),
                    view.getPaddingEnd(),
                    view.getPaddingBottom());
        } else if (BaseSuggestionViewProperties.USE_LARGE_DECORATION == propertyKey) {
            view.setUseLargeDecorationIcon(
                    model.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION));
        }
    }

    /** Binds action icons for the suggestion view. */
    private static <T extends View> void bindActionButtons(
            PropertyModel model, BaseSuggestionView<T> view, List<Action> actions) {
        final int actionCount = actions != null ? actions.size() : 0;
        view.setActionButtonsCount(actionCount);

        // Drawable retrieved once here (expensive) and will be copied multiple times (cheap).
        final List<ActionButtonView> actionViews = view.getActionButtons();
        for (int index = 0; index < actionCount; index++) {
            final ActionButtonView actionView = actionViews.get(index);
            final Action action = actions.get(index);
            actionView.setOnClickListener(v -> action.callback.run());
            actionView.setContentDescription(action.accessibilityDescription);
            actionView.enableShowOnlyOnFocus(action.showOnlyOnFocus);
            TooltipCompat.setTooltipText(actionView, action.accessibilityDescription);
            updateIcon(
                    actionView,
                    action.icon,
                    ChromeColors.getPrimaryIconTintRes(isIncognito(model)));

            actionView.setAccessibilityDelegate(
                    new AccessibilityDelegate() {
                        @Override
                        public void onInitializeAccessibilityNodeInfo(
                                View host, AccessibilityNodeInfo info) {
                            super.onInitializeAccessibilityNodeInfo(host, info);
                            info.addAction(AccessibilityAction.ACTION_CLICK);
                        }

                        @Override
                        public boolean performAccessibilityAction(
                                View host, int accessibilityAction, @Nullable Bundle arguments) {
                            if (accessibilityAction == AccessibilityNodeInfo.ACTION_CLICK
                                    && action.onClickAnnouncement != null) {
                                actionView.setContentDescription(action.onClickAnnouncement);
                                actionView.sendAccessibilityEvent(
                                        AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
                            }
                            return super.performAccessibilityAction(
                                    host, accessibilityAction, arguments);
                        }
                    });
        }
    }

    private void updateColorScheme(PropertyModel model, BaseSuggestionView<T> view) {
        maybeResetCachedFocusableDrawableState(model, view);
        updateSuggestionIcon(model, view);
        applySelectableBackground(model, view, getResourceProvider(model));

        final List<Action> actions = model.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        // Setting ACTION_BUTTONS and updating actionViews can happen later. Appropriate color
        // scheme will be applied then.
        if (actions == null) return;

        final List<ActionButtonView> actionViews = view.getActionButtons();
        for (int index = 0; index < actionViews.size(); index++) {
            ImageView actionView = actionViews.get(index);

            updateIcon(
                    actionView,
                    actions.get(index).icon,
                    ChromeColors.getPrimaryIconTintRes(isIncognito(model)));
        }
    }

    /**
     * @param model Property model containing suggestion view properties.
     * @return Whether the current {@link BrandedColorScheme} is INCOGNITO.
     */
    private static boolean isIncognito(PropertyModel model) {
        return model.get(SuggestionCommonProperties.COLOR_SCHEME) == BrandedColorScheme.INCOGNITO;
    }

    /** Updates attributes of decorated suggestion icon. */
    private static <T extends View> void updateSuggestionIcon(
            PropertyModel model, BaseSuggestionView<T> baseView) {
        final ImageView rciv = baseView.decorationIcon;
        final OmniboxDrawableState sds = model.get(BaseSuggestionViewProperties.ICON);

        if (sds != null) {
            // Ensure the decoration icon size does not exceed the maximum edge size.
            OmniboxResourceProvider resourceProvider = getResourceProvider(model);
            int edgeSize =
                    sds.isLarge
                            ? resourceProvider.getEdgeSizeLargeIcon()
                            : resourceProvider.getEdgeSize();
            boolean isTall = sds.drawable.getIntrinsicHeight() > sds.drawable.getIntrinsicWidth();
            rciv.getLayoutParams().width = isTall ? ViewGroup.LayoutParams.WRAP_CONTENT : edgeSize;
            rciv.getLayoutParams().height = isTall ? edgeSize : ViewGroup.LayoutParams.WRAP_CONTENT;

            // Note: ImageView, unlike other View types, includes logic to scale its bounds
            // proportionally to its image aspect ratio. This guarantees behavior consistent with
            // RoundedCornerImageView, dp-accurate rounding and hardware acceleration.
            // The view bound adjustment is controlled by the following three lines.
            rciv.setAdjustViewBounds(true);
            rciv.setMaxWidth(edgeSize);
            rciv.setMaxHeight(edgeSize);

            rciv.setClipToOutline(sds.useRoundedCorners);
            baseView.decorationIconOutline.setRadius(
                    sds.isLarge
                            ? resourceProvider.getLargeIconRoundingRadius()
                            : resourceProvider.getSmallIconRoundingRadius());
        }

        rciv.setVisibility(sds == null ? View.GONE : View.VISIBLE);
        updateIcon(rciv, sds, ChromeColors.getSecondaryIconTintRes(isIncognito(model)));
    }

    /**
     * Access the BaseSuggestionViewMetadata for the given view, creating and attaching a new one if
     * none is currently associated.
     */
    private static BaseSuggestionViewMetadata ensureViewMetadata(View view) {
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
     * <p>The method internally stores the ConstantState for the drawable to be returned to
     * accelerate creation of subsequent objects.
     *
     * @param model A property model to look up relevant properties.
     * @param view A view that receives background.
     * @param resourceProvider Provider for omnibox resources.
     */
    public static void applySelectableBackground(
            PropertyModel model, View view, OmniboxResourceProvider resourceProvider) {
        // Use a throwaway metadata object if caching is off to simplify branching; the performance
        // difference will still manifest because it's not persisted.
        BaseSuggestionViewMetadata metadata = ensureViewMetadata(view);
        Drawable background;

        if (sFocusableDrawableState == null) {
            @ColorInt
            int suggestionBgColor =
                    resourceProvider.getSuggestionBackgroundColor(
                            model.get(SuggestionCommonProperties.FUSEBOX_LAYOUT_MODE),
                            /* isDropdownContainer= */ false);
            background = resourceProvider.getStatefulSuggestionBackground(suggestionBgColor);
            sFocusableDrawableState = background.getConstantState();
        } else {
            if (sFocusableDrawableState == metadata.backgroundConstantState) return;
            background = sFocusableDrawableState.newDrawable();
        }

        view.setBackground(background);
        metadata.backgroundConstantState = sFocusableDrawableState;
    }

    /**
     * Checks whether cached FocusableDrawableState should be reset.
     *
     * <p>TODO(ender): Relocate this to appropriate OmniboxResourceManager class.
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

    /** Updates image view using supplied drawable state object. */
    private static void updateIcon(
            ImageView view, OmniboxDrawableState sds, @ColorRes int tintRes) {
        if (sds == null) {
            // Release any drawable that is still attached to this view to reclaim memory.
            view.setImageDrawable(null);
            return;
        }

        ColorStateList tint = null;
        if (sds.allowTint) {
            tint = view.getContext().getColorStateList(tintRes);
        }

        view.setImageDrawable(sds.drawable);
        view.setForegroundTintList(tint);
        ImageViewCompat.setImageTintList(view, tint);
    }

    private static void updateRounding(PropertyModel model, BaseSuggestionView<?> view) {
        @PositionalMode
        int positionalMode = model.get(SuggestionCommonProperties.BG_POSITIONAL_MODE);
        @RoundSides int roundSides = model.get(SuggestionCommonProperties.BG_ROUND_SIDES);
        boolean roundTopEdge =
                (roundSides == RoundSides.TOP_AND_BOTTOM)
                        && (positionalMode == PositionalMode.TOP
                                || positionalMode == PositionalMode.SINGLE);
        boolean roundBottomEdge =
                (roundSides == RoundSides.TOP_AND_BOTTOM || roundSides == RoundSides.BOTTOM_ONLY)
                        && (positionalMode == PositionalMode.BOTTOM
                                || positionalMode == PositionalMode.SINGLE);
        view.setRoundingEdges(roundTopEdge, roundBottomEdge);
    }

    public static void resetCachedResources() {
        sFocusableDrawableState = null;
    }

    /** Returns the cached ConstantState for testing. */
    public static @Nullable ConstantState getFocusableDrawableStateForTesting() {
        return sFocusableDrawableState;
    }

    protected abstract void bindContent(
            PropertyModel model, T contentView, PropertyKey propertyKey);
}
