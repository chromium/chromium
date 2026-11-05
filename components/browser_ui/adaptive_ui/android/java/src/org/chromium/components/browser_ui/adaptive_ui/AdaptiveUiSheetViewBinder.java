// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/**
 * Stateless view binder that maps visual {@link AdaptiveUiProperties} keys onto the inflated {@code
 * R.layout.adaptive_ui_sheet} view hierarchy via {@link
 * org.chromium.ui.modelutil.PropertyModelChangeProcessor}.
 *
 * <p>Event-time keys ({@link AdaptiveUiProperties#ON_PRIMARY_BUTTON_CLICKED}, {@link
 * AdaptiveUiProperties#ON_SECONDARY_BUTTON_CLICKED}, {@link
 * AdaptiveUiProperties#PRIMARY_BUTTON_KEEPS_OPEN}, {@link
 * AdaptiveUiProperties#SECONDARY_BUTTON_KEEPS_OPEN}, and {@link
 * AdaptiveUiProperties#BACK_PRESS_HANDLER}) are read on interaction by {@link
 * AdaptiveUiSheetCoordinator}, and {@link AdaptiveUiProperties#DIALOG_CONTENT_DESCRIPTION} applies
 * only to dialogs.
 */
@NullMarked
final class AdaptiveUiSheetViewBinder {
    /**
     * Keys handled at interaction time by {@link AdaptiveUiSheetCoordinator} or used exclusively by
     * dialogs.
     */
    static final Set<PropertyKey> NON_VISUAL_SHEET_KEYS =
            Set.of(
                    AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                    AdaptiveUiProperties.BACK_PRESS_HANDLER,
                    AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                    AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN);

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
        } else if (key == AdaptiveUiProperties.SHOW_CLOSE_BUTTON) {
            view.findViewById(R.id.adaptive_ui_sheet_close_button)
                    .setVisibility(
                            model.get(AdaptiveUiProperties.SHOW_CLOSE_BUTTON)
                                    ? View.VISIBLE
                                    : View.GONE);
            updateHeaderVisibility(view);
        } else if (key == AdaptiveUiProperties.CUSTOM_VIEW) {
            bindCustomView(
                    view.findViewById(R.id.adaptive_ui_sheet_custom_view_container),
                    model.get(AdaptiveUiProperties.CUSTOM_VIEW));
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
        } else {
            assert NON_VISUAL_SHEET_KEYS.contains(key)
                    : "Unhandled AdaptiveUiProperties key in AdaptiveUiSheetViewBinder: " + key;
        }
    }

    private static void updateHeaderVisibility(View root) {
        View header = root.findViewById(R.id.adaptive_ui_sheet_header);
        boolean visible =
                root.findViewById(R.id.adaptive_ui_sheet_icon).getVisibility() == View.VISIBLE
                        || root.findViewById(R.id.adaptive_ui_sheet_title).getVisibility()
                                == View.VISIBLE
                        || root.findViewById(R.id.adaptive_ui_sheet_close_button).getVisibility()
                                == View.VISIBLE;
        header.setVisibility(visible ? View.VISIBLE : View.GONE);
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

    private AdaptiveUiSheetViewBinder() {}
}
