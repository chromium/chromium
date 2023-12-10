// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.tile;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the TileView. */
class TileViewMediator {
    private final PropertyModel mModel;

    /** Create new TileViewMediator object. */
    TileViewMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Set the Tile Title.
     * The title will be shown in at most TITLE_LINES lines.
     *
     * @param title Title to be displayed.
     */
    void setTitle(String title) {
        mModel.set(TileViewProperties.TITLE, title);
    }

    /**
     * Set the max number of lines for the TextView showing the TITLE.
     *
     * @param lines Maximum number of lines that can be used to present the Title.
     */
    void setTitleLines(int lines) {
        mModel.set(TileViewProperties.TITLE_LINES, lines);
    }

    /**
     * Set the Tile Icon.
     *
     * @param icon Icon to show within the tile.
     */
    void setIcon(Drawable icon) {
        mModel.set(TileViewProperties.ICON, icon);
    }

    /**
     * Set whether the Icon Badge should be visible.
     *
     * @param badgeVisible Whether icon badge should be visible.
     */
    void setBadgeVisible(boolean badgeVisible) {
        mModel.set(TileViewProperties.BADGE_VISIBLE, badgeVisible);
    }

    /**
     * Set whether Tile should present a large Icon.
     *
     * @param showLargeIcon Whether Tile Icon should be large.
     */
    void setShowLargeIcon(boolean showLargeIcon) {
        mModel.set(TileViewProperties.SHOW_LARGE_ICON, showLargeIcon);
    }

    /**
     * Set the rounding radius of the embedded icon.
     * The supplied radius value is clipped at half the smaller of the (width, height) dimensions,
     * so that supplying an exceptionally large value will always guarantee the view to be round.
     *
     * Radius is only applied when displaying small icons. Large icons are implicitly rounded to
     * fill in the view.
     *
     * @param roundingRadiusPx Rounding radius (in pixels), or 0 to disable rounding.
     */
    public void setSmallIconRoundingRadiusPx(int roundingRadiusPx) {
        mModel.set(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS, roundingRadiusPx);
    }

    /**
     * Set the handler receiving click events.
     *
     * @param listener Handler receiving click events.
     */
    void setOnClickListener(View.OnClickListener listener) {
        mModel.set(TileViewProperties.ON_CLICK, listener);
    }

    /**
     * Set the handler receiving long click events.
     *
     * @param listener Handler receiving long click events.
     */
    void setOnLongClickListener(View.OnLongClickListener listener) {
        mModel.set(TileViewProperties.ON_LONG_CLICK, listener);
    }

    /**
     * Set the handler receiving context menu create events.
     *
     * @param listener Handler receiving context menu create events.
     */
    void setOnCreateContextMenuListener(View.OnCreateContextMenuListener listener) {
        mModel.set(TileViewProperties.ON_CREATE_CONTEXT_MENU, listener);
    }

    /**
     * Set the Accessibility Content Description.
     *
     * @param description Text used by Talkback to announce selection.
     */
    void setContentDescription(CharSequence description) {
        mModel.set(TileViewProperties.CONTENT_DESCRIPTION, description);
    }
}
