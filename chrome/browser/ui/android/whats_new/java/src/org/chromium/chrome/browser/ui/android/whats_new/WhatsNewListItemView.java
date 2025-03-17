// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** View for the for an individual list item in the What's New page's feature list. */
@NullMarked
public class WhatsNewListItemView extends RelativeLayout {
    private TextView mTitle;
    private TextView mDescription;
    private ImageView mImage;

    /**
     * Constructor a WhatsNewListItemView
     *
     * @param context The {@link Context} to use.
     * @param attrs Attributes from the XML layout inflation.
     */
    public WhatsNewListItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.whats_new_item_title);
        mDescription = findViewById(R.id.whats_new_item_description);
        mImage = findViewById(R.id.whats_new_item_image);
    }

    void setTitle(String title) {
        mTitle.setText(title);
    }

    void setDescription(String description) {
        mDescription.setText(description);
    }

    void setImage(@DrawableRes int imageId) {
        mImage.setImageResource(imageId);
    }
}
