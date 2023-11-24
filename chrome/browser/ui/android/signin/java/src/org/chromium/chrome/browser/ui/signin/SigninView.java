// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.ui.UiUtils;
import org.chromium.ui.drawable.AnimationLooper;

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
    private DualControlLayout mButtonBar;
    private Button mAcceptButton;
    private Button mRefuseButton;
    private Button mMoreButton;
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
        mMoreButton = findViewById(R.id.more_button);

        mRefuseButton = DualControlLayout.createButtonForLayout(getContext(), false, "", null);
        mRefuseButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mAcceptButton = DualControlLayout.createButtonForLayout(getContext(), true, "", null);
        mAcceptButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mButtonBar = findViewById(R.id.dual_control_button_bar);
        mButtonBar.addView(mAcceptButton);
        mButtonBar.addView(mRefuseButton);
        mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.APART);

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

    DualControlLayout getButtonBar() {
        return mButtonBar;
    }

    Button getAcceptButton() {
        return mAcceptButton;
    }

    Button getRefuseButton() {
        return mRefuseButton;
    }

    Button getMoreButton() {
        return mMoreButton;
    }

    void startAnimations() {
        mAnimationLooper.start();
    }

    void stopAnimations() {
        mAnimationLooper.stop();
    }

    static Drawable getExpandArrowDrawable(Context context) {
        return UiUtils.getTintedDrawable(
                context,
                R.drawable.ic_expand_more_black_24dp,
                R.color.default_icon_color_tint_list);
    }

    static Drawable getCheckmarkDrawable(Context context) {
        return AppCompatResources.getDrawable(context, R.drawable.ic_check_googblue_24dp);
    }
}
