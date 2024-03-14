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

import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.DualControlLayout.ButtonType;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.UiUtils;
import org.chromium.ui.drawable.AnimationLooper;

/** View that wraps signin screen and caches references to UI elements. */
class SigninView extends LinearLayout {

    /** Registers {@param view}'s text in a consent text tracker. */
    interface ConsentTextUpdater {
        void updateConsentText(TextView view);
    }

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

    private OnClickListener mAcceptOnClickListener;
    private ConsentTextUpdater mAcceptConsentTextUpdater;

    private @DualControlLayout.ButtonType int mAcceptButtonType;

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

        ImageView headerImage = findViewById(R.id.signin_header_image);
        mAnimationLooper = new AnimationLooper(headerImage.getDrawable());

        createButtons();
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

    void setAcceptOnClickListener(OnClickListener listener) {
        this.mAcceptOnClickListener = listener;
    }

    /**
     * Since buttons can be dynamically replaced, it delegates the work to the actual listener in
     * {@link SigninView#mAcceptOnClickListener}
     */
    private void acceptOnClickListenerProxy(View view) {
        if (this.mAcceptOnClickListener == null) {
            return;
        }

        if (this.mAcceptButtonType == ButtonType.PRIMARY_FILLED) {
            MinorModeHelper.recordButtonClicked(
                    MinorModeHelper.SyncButtonClicked.SYNC_OPT_IN_NOT_EQUAL_WEIGHTED);
        } else {
            MinorModeHelper.recordButtonClicked(
                    MinorModeHelper.SyncButtonClicked.SYNC_OPT_IN_EQUAL_WEIGHTED);
        }

        this.mAcceptOnClickListener.onClick(view);
    }

    private void refuseOnClickListener(View view) {
        if (this.mAcceptButtonType == ButtonType.PRIMARY_FILLED) {
            MinorModeHelper.recordButtonClicked(
                    MinorModeHelper.SyncButtonClicked.SYNC_CANCEL_NOT_EQUAL_WEIGHTED);
        } else {
            MinorModeHelper.recordButtonClicked(
                    MinorModeHelper.SyncButtonClicked.SYNC_CANCEL_EQUAL_WEIGHTED);
        }
    }

    void settingsClicked() {
        if (this.mAcceptButtonType == ButtonType.PRIMARY_FILLED) {
            MinorModeHelper.recordButtonClicked(
                    MinorModeHelper.SyncButtonClicked.SYNC_SETTINGS_NOT_EQUAL_WEIGHTED);
        } else {
            MinorModeHelper.recordButtonClicked(
                    MinorModeHelper.SyncButtonClicked.SYNC_SETTINGS_EQUAL_WEIGHTED);
        }
    }

    /**
     * {@param updater} once executed should record consent text of given TextView (see {@link
     * SigninView.ConsentTextUpdater}).
     */
    void setAcceptConsentTextUpdater(ConsentTextUpdater updater) {
        this.mAcceptConsentTextUpdater = updater;
        updateAcceptConsentText();
    }

    /**
     * Must be called on every recreated button so that its text is recorded in consent text
     * tracker.
     */
    private void updateAcceptConsentText() {
        if (this.mAcceptButton == null || this.mAcceptConsentTextUpdater == null) {
            return;
        }

        this.mAcceptConsentTextUpdater.updateConsentText(this.mAcceptButton);
    }

    private void createButtons() {
        mRefuseButton =
                DualControlLayout.createButtonForLayout(
                        getContext(),
                        DualControlLayout.ButtonType.SECONDARY,
                        "",
                        this::refuseOnClickListener);
        mRefuseButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        @DualControlLayout.ButtonType
        int acceptButtonType =
                SigninFeatureMap.isEnabled(
                                SigninFeatures.MINOR_MODE_RESTRICTIONS_FOR_HISTORY_SYNC_OPT_IN)
                        ? DualControlLayout.ButtonType.PRIMARY_TEXT
                        : DualControlLayout.ButtonType.PRIMARY_FILLED;
        mAcceptButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), acceptButtonType, "", this::acceptOnClickListenerProxy);
        mAcceptButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mButtonBar = findViewById(R.id.dual_control_button_bar);
        addButtonsToButtonBar();
    }

    /**
     * Removes buttons from button bar and readds them keeping their configuration. Buttons are
     * themed according to {@link screenMode} param.
     */
    void recreateButtons(@ScreenMode int screenMode) {
        mButtonBar.removeAllViews();

        Button oldButton = mAcceptButton;

        mAcceptButtonType =
                screenMode == ScreenMode.UNRESTRICTED
                        ? DualControlLayout.ButtonType.PRIMARY_FILLED
                        : DualControlLayout.ButtonType.PRIMARY_TEXT;

        mAcceptButton =
                DualControlLayout.createButtonForLayout(
                        getContext(),
                        mAcceptButtonType,
                        oldButton.getText().toString(),
                        this::acceptOnClickListenerProxy);
        mAcceptButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mAcceptButton.setEnabled(oldButton.isEnabled());
        updateAcceptConsentText();

        // This button is not changed, make it unconditionally visible.
        mRefuseButton.setVisibility(View.VISIBLE);
        addButtonsToButtonBar();

        // Only at this point buttons were made visible and added to the button bar, so record the
        // displayed button type.
        if (mAcceptButtonType == ButtonType.PRIMARY_FILLED) {
            MinorModeHelper.recordButtonsShown(
                    MinorModeHelper.SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED);
        } else {
            MinorModeHelper.recordButtonsShown(MinorModeHelper.SyncButtonsType.SYNC_EQUAL_WEIGHTED);
        }
    }

    private void addButtonsToButtonBar() {
        mButtonBar.addView(mAcceptButton);
        mButtonBar.addView(mRefuseButton);
        mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.APART);
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
