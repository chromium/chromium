// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.graphics.Bitmap;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link ViewHolder} responsible for building and setting properties on the tiles. */
class TileViewHolder extends ViewHolder {
    /** Creates an instance of a {@link TileViewHolder}. */
    protected TileViewHolder(View itemView) {
        super(itemView);
    }

    /**
     * Binds the currently held {@link View} to {@code item}.
     * @param properties The shared {@link PropertyModel} all items can access.
     * @param tile       The {@link ListItem} to visually represent in this {@link ViewHolder}.
     */
    public void bind(PropertyModel properties, ImageTile tile) {
        TextView title = itemView.findViewById(R.id.title);
        title.setText(tile.displayTitle);
        itemView.setOnClickListener(
                v -> {
                    properties.get(TileListProperties.CLICK_CALLBACK).onResult(tile);
                });

        showBitmap(null);
        properties
                .get(TileListProperties.VISUALS_CALLBACK)
                .getVisuals(
                        tile,
                        bitmaps -> {
                            showBitmap(
                                    bitmaps != null && !bitmaps.isEmpty() ? bitmaps.get(0) : null);
                        });
    }

    private void showBitmap(@Nullable Bitmap bitmap) {
        final ImageView thumbnail = itemView.findViewById(R.id.thumbnail);
        final ImageView overlay = itemView.findViewById(R.id.gradient_overlay);
        if (bitmap == null) {
            thumbnail.setImageDrawable(
                    new ColorDrawable(
                            thumbnail.getContext().getColor(R.color.image_loading_color)));
        } else {
            thumbnail.setImageBitmap(bitmap);
        }
        overlay.setVisibility(bitmap == null ? View.GONE : View.VISIBLE);
    }

    /**
     * Gives subclasses a chance to free up expensive resources when this {@link ViewHolder} is no
     * longer attached to the parent {@link RecyclerView}.
     */
    public void recycle() {}
}
