// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.CloseButtonVisibility;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/**
 * Stateless view binder that maps visual {@link AdaptiveUiProperties} keys onto the inflated {@code
 * R.layout.adaptive_ui_sheet} view hierarchy via {@link
 * org.chromium.ui.modelutil.PropertyModelChangeProcessor}.
 *
 * <p>Event-time and {@code BottomSheetContent} metadata keys ({@link
 * AdaptiveUiProperties#SHEET_OPENED_ACCESSIBILITY_STRING_ID}, {@link
 * AdaptiveUiProperties#SHEET_CLOSED_ACCESSIBILITY_STRING_ID}, {@link
 * AdaptiveUiProperties#SHEET_INITIAL_STATE}, {@link AdaptiveUiProperties#SHEET_USER_INITIATED},
 * {@link AdaptiveUiProperties#SHEET_USER_CRITICAL}, {@link
 * AdaptiveUiProperties#SHEET_HALF_HEIGHT_RATIO}, {@link
 * AdaptiveUiProperties#SHEET_FULL_HEIGHT_RATIO}, {@link AdaptiveUiProperties#SHOW_SHEET_HANDLEBAR},
 * {@link AdaptiveUiProperties#CUSTOM_VIEW_SCROLLS_ITSELF}, {@link
 * AdaptiveUiProperties#PRIMARY_BUTTON_KEEPS_OPEN}, {@link
 * AdaptiveUiProperties#SECONDARY_BUTTON_BEHAVIOR}, {@link
 * AdaptiveUiProperties#BUTTON_TAP_PROTECTION_PERIOD_MS}, {@link
 * AdaptiveUiProperties#GESTURE_DISMISSAL_DISABLED}, and {@link
 * AdaptiveUiProperties#BACK_PRESS_HANDLER}) are read directly by {@link
 * AdaptiveUiSheetCoordinator}. {@link AdaptiveUiProperties#DELEGATE} is invoked by {@link
 * AdaptiveUiSheetCoordinator} ({@link AdaptiveUiProperties.Delegate#onPrimaryButtonClicked} and
 * {@link AdaptiveUiProperties.Delegate#onSecondaryButtonClicked}) and {@link
 * AdaptiveUiControllerImpl} ({@link AdaptiveUiProperties.Delegate#onShown} and {@link
 * AdaptiveUiProperties.Delegate#onDismissed}). {@link
 * AdaptiveUiProperties#DIALOG_CONTENT_DESCRIPTION} is dialog-only and ignored by sheets.
 */
@NullMarked
final class AdaptiveUiSheetViewBinder {
    /**
     * Keys read directly by {@link AdaptiveUiSheetCoordinator} (for interaction or {@code
     * BottomSheetContent} metadata), {@link AdaptiveUiProperties#DELEGATE} (invoked by {@link
     * AdaptiveUiSheetCoordinator} and {@link AdaptiveUiControllerImpl}), or keys used exclusively
     * by dialogs.
     */
    static final Set<PropertyKey> NON_VISUAL_SHEET_KEYS =
            Set.of(
                    AdaptiveUiProperties.DELEGATE,
                    AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                    AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                    AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                    AdaptiveUiProperties.SHEET_INITIAL_STATE,
                    AdaptiveUiProperties.SHEET_USER_INITIATED,
                    AdaptiveUiProperties.SHEET_USER_CRITICAL,
                    AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO,
                    AdaptiveUiProperties.SHEET_FULL_HEIGHT_RATIO,
                    AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR,
                    AdaptiveUiProperties.BACK_PRESS_HANDLER,
                    AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF,
                    AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED,
                    AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                    AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                    AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS);

    static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == AdaptiveUiProperties.HEADER_ICON) {
            ImageView icon = view.findViewById(R.id.adaptive_ui_sheet_icon);
            Drawable drawable = model.get(AdaptiveUiProperties.HEADER_ICON);
            icon.setImageDrawable(drawable);
            icon.setVisibility(drawable == null ? View.GONE : View.VISIBLE);
            updateHeaderVisibility(view);
        } else if (key == AdaptiveUiProperties.TITLE) {
            bindText(
                    view.findViewById(R.id.adaptive_ui_sheet_title),
                    model.get(AdaptiveUiProperties.TITLE));
            updateHeaderVisibility(view);
        } else if (key == AdaptiveUiProperties.DESCRIPTION) {
            TextView description = view.findViewById(R.id.adaptive_ui_sheet_description);
            bindText(description, model.get(AdaptiveUiProperties.DESCRIPTION));
            UiUtils.maybeSetLinkMovementMethod(description);
        } else if (key == AdaptiveUiProperties.FOOTER_MESSAGE) {
            bindText(
                    view.findViewById(R.id.adaptive_ui_sheet_footer_message),
                    model.get(AdaptiveUiProperties.FOOTER_MESSAGE));
            updatePrimaryButtonTopMargin(view);
        } else if (key == AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY) {
            boolean visible =
                    model.get(AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY)
                            == CloseButtonVisibility.SHOWN;
            view.findViewById(R.id.adaptive_ui_sheet_close_button)
                    .setVisibility(visible ? View.VISIBLE : View.GONE);
            updateHeaderVisibility(view);
        } else if (key == AdaptiveUiProperties.CUSTOM_VIEW) {
            bindCustomView(
                    view.findViewById(R.id.adaptive_ui_sheet_custom_view_container),
                    model.get(AdaptiveUiProperties.CUSTOM_VIEW));
            updatePrimaryButtonTopMargin(view);
        } else if (key == AdaptiveUiProperties.PRIMARY_BUTTON_TEXT) {
            bindText(
                    view.findViewById(R.id.adaptive_ui_sheet_primary_button),
                    model.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT));
        } else if (key == AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED) {
            view.findViewById(R.id.adaptive_ui_sheet_primary_button)
                    .setEnabled(!model.get(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED));
        } else if (key == AdaptiveUiProperties.SECONDARY_BUTTON_TEXT) {
            bindText(
                    view.findViewById(R.id.adaptive_ui_sheet_secondary_button),
                    model.get(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT));
        } else if (key == AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED) {
            view.findViewById(R.id.adaptive_ui_sheet_secondary_button)
                    .setEnabled(!model.get(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED));
        } else if (key == AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY) {
            if (model.get(AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY)) {
                setupFilterTouchForSecurity(
                        view.findViewById(R.id.adaptive_ui_sheet_primary_button));
                setupFilterTouchForSecurity(
                        view.findViewById(R.id.adaptive_ui_sheet_secondary_button));
            }
        } else {
            assert NON_VISUAL_SHEET_KEYS.contains(key)
                    : "Unhandled AdaptiveUiProperties key in AdaptiveUiSheetViewBinder: " + key;
        }
    }

    private static void updateHeaderVisibility(View root) {
        View header = root.findViewById(R.id.adaptive_ui_sheet_header);
        View icon = root.findViewById(R.id.adaptive_ui_sheet_icon);
        TextView title = root.findViewById(R.id.adaptive_ui_sheet_title);
        View close = root.findViewById(R.id.adaptive_ui_sheet_close_button);
        boolean iconVisible = icon.getVisibility() == View.VISIBLE;
        boolean titleVisible = title.getVisibility() == View.VISIBLE;
        boolean closeVisible = close.getVisibility() == View.VISIBLE;
        header.setVisibility(
                iconVisible || titleVisible || closeVisible ? View.VISIBLE : View.GONE);

        Resources res = root.getResources();
        int verticalMargin = res.getDimensionPixelSize(R.dimen.adaptive_ui_sheet_vertical_margin);
        int touchTarget = res.getDimensionPixelSize(R.dimen.min_touch_target_size);
        int iconSize = res.getDimensionPixelSize(R.dimen.adaptive_ui_sheet_icon_size);

        // Keep the icon-to-title gap only when the title is below the icon, so an icon-only
        // header adds no space above the body.
        ViewGroup.MarginLayoutParams iconParams =
                (ViewGroup.MarginLayoutParams) icon.getLayoutParams();
        iconParams.bottomMargin = titleVisible ? verticalMargin : 0;
        icon.setLayoutParams(iconParams);

        // The close button sits on the first line: centered on the icon when it is shown,
        // otherwise on the title, which is then made as tall as the button.
        title.setMinHeight(closeVisible && !iconVisible ? touchTarget : 0);
        ViewGroup.MarginLayoutParams closeParams =
                (ViewGroup.MarginLayoutParams) close.getLayoutParams();
        closeParams.topMargin =
                iconVisible ? verticalMargin - (touchTarget - iconSize) / 2 : verticalMargin;
        close.setLayoutParams(closeParams);
    }

    private static void bindText(TextView textView, @Nullable CharSequence text) {
        textView.setText(text);
        textView.setVisibility(TextUtils.isEmpty(text) ? View.GONE : View.VISIBLE);
    }

    private static void bindCustomView(ViewGroup container, @Nullable View customView) {
        container.removeAllViews();
        if (customView != null) {
            UiUtils.removeViewFromParent(customView);
            container.addView(customView);
        }
        container.setVisibility(customView == null ? View.GONE : View.VISIBLE);
    }

    /**
     * Clears the primary button's top margin when the custom view sits directly above the buttons,
     * and restores {@code R.dimen.adaptive_ui_sheet_vertical_margin} otherwise.
     */
    private static void updatePrimaryButtonTopMargin(View root) {
        boolean customViewDirectlyAboveButtons =
                root.findViewById(R.id.adaptive_ui_sheet_custom_view_container).getVisibility()
                                == View.VISIBLE
                        && root.findViewById(R.id.adaptive_ui_sheet_footer_message).getVisibility()
                                != View.VISIBLE;
        View primaryButton = root.findViewById(R.id.adaptive_ui_sheet_primary_button);
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) primaryButton.getLayoutParams();
        params.topMargin =
                customViewDirectlyAboveButtons
                        ? 0
                        : root.getResources()
                                .getDimensionPixelSize(R.dimen.adaptive_ui_sheet_vertical_margin);
        primaryButton.setLayoutParams(params);
    }

    // Only obscured touches are swallowed. Other touches return false and reach the button's own
    // click handling, which calls performClick(), so this listener never handles a click.
    @SuppressWarnings("ClickableViewAccessibility")
    private static void setupFilterTouchForSecurity(View button) {
        button.setFilterTouchesWhenObscured(true);
        button.setOnTouchListener(
                (v, ev) -> (ev.getFlags() & MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED) != 0);
    }

    private AdaptiveUiSheetViewBinder() {}
}
