// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.UiUtils;
import org.chromium.ui.drawable.AnimationLooper;
import org.chromium.ui.widget.ButtonCompat;

/** View that wraps signin screen and caches references to UI elements. */
class SigninView extends LinearLayout {
    private SigninScrollView mScrollView;
    private TextView mTitle;
    private View mAccountPicker;
    private ImageView mAccountImage;
    private TextView mAccountTextPrimary;
    private TextView mAccountTextSecondary;
    private ImageView mAccountPickerEndImage;
    private TextView mSyncTitle;
    private TextView mSyncDescription;
    private TextView mDetailsDescription;
    private ButtonCompat mAcceptButton;
    private Button mRefuseButton;
    private Button mMoreButton;
    private View mAcceptButtonEndPadding;
    private AnimationLooper mAnimationLooper;

    public SigninView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.signin_scroll_view);
        mTitle = findViewById(R.id.signin_title);
        mAccountPicker = findViewById(R.id.signin_account_picker);
        mAccountImage = findViewById(R.id.account_image);
        mAccountTextPrimary = findViewById(R.id.account_text_primary);
        mAccountTextSecondary = findViewById(R.id.account_text_secondary);
        mAccountPickerEndImage = findViewById(R.id.account_picker_end_image);
        mSyncTitle = findViewById(R.id.signin_sync_title);
        mSyncDescription = findViewById(R.id.signin_sync_description);
        mDetailsDescription = findViewById(R.id.signin_details_description);
        mAcceptButton = findViewById(R.id.positive_button);
        mRefuseButton = findViewById(R.id.negative_button);
        mMoreButton = findViewById(R.id.more_button);
        mAcceptButtonEndPadding = findViewById(R.id.positive_button_end_padding);

        ImageView headerImage = findViewById(R.id.signin_header_image);
        mAnimationLooper = new AnimationLooper(headerImage.getDrawable());
    }

    SigninScrollView getScrollView() {
        return mScrollView;
    }

    TextView getTitleView() {
        return mTitle;
    }

    View getAccountPickerView() {
        return mAccountPicker;
    }

    ImageView getAccountImageView() {
        return mAccountImage;
    }

    TextView getAccountTextPrimary() {
        return mAccountTextPrimary;
    }

    TextView getAccountTextSecondary() {
        return mAccountTextSecondary;
    }

    ImageView getAccountPickerEndImageView() {
        return mAccountPickerEndImage;
    }

    TextView getSyncTitleView() {
        return mSyncTitle;
    }

    TextView getSyncDescriptionView() {
        return mSyncDescription;
    }

    TextView getDetailsDescriptionView() {
        return mDetailsDescription;
    }

    ButtonCompat getAcceptButton() {
        return mAcceptButton;
    }

    Button getRefuseButton() {
        return mRefuseButton;
    }

    Button getMoreButton() {
        return mMoreButton;
    }

    View getAcceptButtonEndPadding() {
        return mAcceptButtonEndPadding;
    }

    void startAnimations() {
        mAnimationLooper.start();
    }

    void stopAnimations() {
        mAnimationLooper.stop();
    }

    static Drawable getExpandArrowDrawable(Context context) {
        return UiUtils.getTintedDrawable(context, R.drawable.ic_expand_more_black_24dp,
                R.color.default_icon_color_tint_list);
    }

    static Drawable getCheckmarkDrawable(Context context) {
        return AppCompatResources.getDrawable(context, R.drawable.ic_check_googblue_24dp);
    }
}
