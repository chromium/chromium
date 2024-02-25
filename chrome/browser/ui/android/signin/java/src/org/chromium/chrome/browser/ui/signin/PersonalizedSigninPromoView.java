// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.ui.widget.ButtonCompat;

/** Container view for personalized signin promos. */
public class PersonalizedSigninPromoView extends FrameLayout {
    private ImageView mImage;
    private ImageButton mDismissButton;
    private TextView mTitle;
    private TextView mDescription;
    private ButtonCompat mPrimaryButton;
    private Button mSecondaryButton;

    public PersonalizedSigninPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.sync_promo_view, this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mImage = findViewById(R.id.sync_promo_image);
        mDismissButton = findViewById(R.id.sync_promo_close_button);
        mPrimaryButton = findViewById(R.id.sync_promo_signin_button);
        mSecondaryButton = findViewById(R.id.sync_promo_choose_account_button);
        mTitle = findViewById(R.id.sync_promo_title);
        mDescription = findViewById(R.id.sync_promo_description);
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

    /** Sets the card's background for R.id.signin_promo_view_wrapper. */
    public void setCardBackgroundResource(@DrawableRes int resId) {
        findViewById(R.id.signin_promo_view_wrapper).setBackgroundResource(resId);
    }
}
