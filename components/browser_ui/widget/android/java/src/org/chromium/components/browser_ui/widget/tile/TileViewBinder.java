// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.tile;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder wiring for the TileView. */
public class TileViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, TileView view, PropertyKey propertyKey) {
        if (propertyKey == TileViewProperties.TITLE) {
            final TextView textView = view.findViewById(R.id.tile_view_title);
            textView.setText(model.get(TileViewProperties.TITLE));
        } else if (propertyKey == TileViewProperties.TITLE_LINES) {
            final TextView textView = view.findViewById(R.id.tile_view_title);
            final int requestedLines = model.get(TileViewProperties.TITLE_LINES);
            textView.setLines(requestedLines > 0 ? requestedLines : 1);
        } else if (propertyKey == TileViewProperties.ICON) {
            final ImageView iconView = view.findViewById(R.id.tile_view_icon);
            iconView.setImageDrawable(model.get(TileViewProperties.ICON));
        } else if (propertyKey == TileViewProperties.ICON_TINT) {
            final ImageView iconView = view.findViewById(R.id.tile_view_icon);
            ImageViewCompat.setImageTintList(iconView, model.get(TileViewProperties.ICON_TINT));
        } else if (propertyKey == TileViewProperties.BADGE_VISIBLE) {
            final View badgeView = view.findViewById(R.id.offline_badge);
            final boolean isVisible = model.get(TileViewProperties.BADGE_VISIBLE);
            badgeView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        } else if (propertyKey == TileViewProperties.SHOW_LARGE_ICON) {
            final boolean useLargeIcon = model.get(TileViewProperties.SHOW_LARGE_ICON);
            final int iconEdgeSize = getIconEdgeSizePx(view.getResources(), useLargeIcon);
            final View iconView = view.findViewById(R.id.tile_view_icon);
            final MarginLayoutParams params = (MarginLayoutParams) iconView.getLayoutParams();
            params.width = iconEdgeSize;
            params.height = iconEdgeSize;
            params.topMargin = getIconTopMarginSizePx(view.getResources(), useLargeIcon);
            iconView.setLayoutParams(params);
            updateRoundingRadius(model, view);
        } else if (propertyKey == TileViewProperties.SMALL_ICON_ROUNDING_RADIUS) {
            updateRoundingRadius(model, view);
        } else if (propertyKey == TileViewProperties.ON_FOCUS_VIA_SELECTION) {
            view.setOnFocusViaSelectionListener(
                    model.get(TileViewProperties.ON_FOCUS_VIA_SELECTION));
        } else if (propertyKey == TileViewProperties.ON_CLICK) {
            view.setOnClickListener(model.get(TileViewProperties.ON_CLICK));
        } else if (propertyKey == TileViewProperties.ON_LONG_CLICK) {
            view.setOnLongClickListener(model.get(TileViewProperties.ON_LONG_CLICK));
        } else if (propertyKey == TileViewProperties.ON_CREATE_CONTEXT_MENU) {
            view.setOnCreateContextMenuListener(
                    model.get(TileViewProperties.ON_CREATE_CONTEXT_MENU));
        } else if (propertyKey == TileViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(TileViewProperties.CONTENT_DESCRIPTION));
        }
    }

    private static void updateRoundingRadius(PropertyModel model, TileView view) {
        int roundingRadiusPx = 0;
        if (model.get(TileViewProperties.SHOW_LARGE_ICON)) {
            // Pick the large icon dimension as a rounding radius. This guarantees that the icon
            // will be fully circular.
            roundingRadiusPx = getIconEdgeSizePx(view.getResources(), /* useLargeIcon= */ true);
        } else {
            roundingRadiusPx = model.get(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS);
            assert roundingRadiusPx >= 0
                    : "Invalid rounding radius specified: must be non-negative integer";
        }
        view.setRoundingRadius(roundingRadiusPx);
    }

    private static int getIconEdgeSizePx(Resources res, boolean useLargeIcon) {
        return res.getDimensionPixelSize(
                useLargeIcon ? R.dimen.tile_view_icon_size : R.dimen.tile_view_icon_size_modern);
    }

    private static int getIconTopMarginSizePx(Resources res, boolean useLargeIcon) {
        return res.getDimensionPixelSize(
                useLargeIcon
                        ? R.dimen.tile_view_icon_background_margin_top_modern
                        : R.dimen.tile_view_icon_margin_top_modern);
    }
}
