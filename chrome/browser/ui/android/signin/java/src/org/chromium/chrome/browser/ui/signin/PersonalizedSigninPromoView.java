// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Container view for personalized signin promos.
 */
public class PersonalizedSigninPromoView extends LinearLayout {
    private ImageView mIllustration;
    private ImageView mImage;
    private ImageButton mDismissButton;
    private TextView mTitle;
    private TextView mDescription;
    private ButtonCompat mPrimaryButton;
    private Button mSecondaryButton;

    public PersonalizedSigninPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIllustration = findViewById(R.id.sync_promo_illustration);
        mImage = findViewById(R.id.sync_promo_image);
        mDismissButton = findViewById(R.id.sync_promo_close_button);
        mPrimaryButton = findViewById(R.id.sync_promo_signin_button);
        mSecondaryButton = findViewById(R.id.sync_promo_choose_account_button);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
            // TODO(crbug.com/1323197): remove new_sync_promo_description or
            // signin_promo_description and sync_promo_title or sync_promo_status_message, if
            // the feature enabled or disabled by default.
            mTitle = findViewById(R.id.sync_promo_title);
            mDescription = findViewById(R.id.new_sync_promo_description);
            findViewById(R.id.signin_promo_description).setVisibility(View.GONE);
        } else {
            mTitle = findViewById(R.id.sync_promo_status_message);
            mDescription = findViewById(R.id.signin_promo_description);
            findViewById(R.id.new_sync_promo_description).setVisibility(View.GONE);
        }
    }

    /**
     * @return A reference to the illustration of the promo.
     */
    public ImageView getIllustration() {
        return mIllustration;
    }

    /**
     * @return A reference to the image of the promo.
     */
    public ImageView getImage() {
        return mImage;
    }

    /**
     * @return A reference to the dismiss button.
     */
    public ImageButton getDismissButton() {
        return mDismissButton;
    }

    /**
     * @return A reference to the title of the sync promo.
     */
    public TextView getTitle() {
        return mTitle;
    }

    /**
     * @return A reference to the description of the promo.
     */
    public TextView getDescription() {
        return mDescription;
    }

    /**
     * @return A reference to the signin button.
     */
    public ButtonCompat getPrimaryButton() {
        return mPrimaryButton;
    }

    /**
     * @return A reference to the choose account button.
     */
    public Button getSecondaryButton() {
        return mSecondaryButton;
    }
}
