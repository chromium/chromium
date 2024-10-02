// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.MaterialCardViewNoShadow;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A promo card view that contains an image view in the top center, a block of short description,
 * two button compat and a close button.
 */
class PromoCardView extends MaterialCardViewNoShadow {
    ImageView mPromoImage;
    TextView mTitle;
    ButtonCompat mPrimaryButton;

    @Nullable TextView mDescription;
    @Nullable ButtonCompat mSecondaryButton;
    @Nullable ChromeImageButton mCloseButton;

    public PromoCardView(Context context) {
        this(context, null);
    }

    public PromoCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        // Find members
        mPromoImage = findViewById(R.id.promo_image);
        mTitle = findViewById(R.id.promo_title);
        mDescription = findViewById(R.id.promo_description);
        mPrimaryButton = findViewById(R.id.promo_primary_button);
        mSecondaryButton = findViewById(R.id.promo_secondary_button);
        mCloseButton = findViewById(R.id.promo_close_button);
    }
}
