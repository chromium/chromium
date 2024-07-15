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

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.signin.metrics.SyncButtonClicked;
import org.chromium.components.signin.metrics.SyncButtonsType;
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

    private @ScreenMode int mScreenMode;

    public SigninView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mScreenMode = ScreenMode.PENDING;
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
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)) {
            mSyncTitle.setText(R.string.signin_sync_title_without_passwords);
        }
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
        switch (mScreenMode) {
            case ScreenMode.RESTRICTED:
            case ScreenMode.DEADLINED:
                MinorModeHelper.recordButtonClicked(SyncButtonClicked.SYNC_OPT_IN_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNRESTRICTED:
                MinorModeHelper.recordButtonClicked(
                        SyncButtonClicked.SYNC_OPT_IN_NOT_EQUAL_WEIGHTED);
                break;
            default:
                // Button not present
        }

        this.mAcceptOnClickListener.onClick(view);
    }

    void refuseButtonClicked() {
        switch (mScreenMode) {
            case ScreenMode.RESTRICTED:
            case ScreenMode.DEADLINED:
                MinorModeHelper.recordButtonClicked(SyncButtonClicked.SYNC_CANCEL_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNRESTRICTED:
                MinorModeHelper.recordButtonClicked(
                        SyncButtonClicked.SYNC_CANCEL_NOT_EQUAL_WEIGHTED);
                break;
            default:
                // Button not present
        }
    }

    void settingsClicked() {
        switch (mScreenMode) {
            case ScreenMode.PENDING:
                MinorModeHelper.recordButtonClicked(
                        SyncButtonClicked.SYNC_SETTINGS_UNKNOWN_WEIGHTED);
                break;
            case ScreenMode.RESTRICTED:
            case ScreenMode.DEADLINED:
                MinorModeHelper.recordButtonClicked(SyncButtonClicked.SYNC_SETTINGS_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNRESTRICTED:
                MinorModeHelper.recordButtonClicked(
                        SyncButtonClicked.SYNC_SETTINGS_NOT_EQUAL_WEIGHTED);
                break;
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
                        getContext(), DualControlLayout.ButtonType.SECONDARY_TEXT, "", null);
        mRefuseButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mAcceptButton =
                DualControlLayout.createButtonForLayout(
                        getContext(),
                        DualControlLayout.ButtonType.PRIMARY_TEXT,
                        "",
                        this::acceptOnClickListenerProxy);
        mAcceptButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mButtonBar = findViewById(R.id.dual_control_button_bar);
        addButtonsToButtonBar();
    }

    /** Recreates buttons for the add account purpose. */
    void recreateAddAccountButtons() {
        recreateButtons(DualControlLayout.ButtonType.PRIMARY_FILLED);
        mScreenMode = ScreenMode.PENDING;
    }

    /**
     * Prepares buttons for the sync consent purpose.
     *
     * <p>If the buttons are already in the request configuration, this is a no-op. Otherwise,
     * buttons are removed and re-added in the right configuration.
     *
     * Buttons in this mode record click and impression metrics.
     *
     * @param screenMode determines the appearance of the buttons.
     */
    void recreateSyncConsentButtons(@ScreenMode int screenMode) {
        if (screenMode == mScreenMode) {
            return;
        }
        mScreenMode = screenMode;

        @DualControlLayout.ButtonType
        int acceptButtonType =
                screenMode == ScreenMode.UNRESTRICTED
                        ? DualControlLayout.ButtonType.PRIMARY_FILLED
                        : DualControlLayout.ButtonType.PRIMARY_TEXT;
        recreateButtons(acceptButtonType);

        // Only at this point buttons were made visible and added to the button bar, so record the
        // displayed button type.
        switch (mScreenMode) {
            case ScreenMode.RESTRICTED:
                MinorModeHelper.recordButtonsShown(
                        SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY);
                break;
            case ScreenMode.UNRESTRICTED:
                MinorModeHelper.recordButtonsShown(SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED);
                break;
            case ScreenMode.DEADLINED:
                MinorModeHelper.recordButtonsShown(
                        SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_DEADLINE);
                break;
        }
    }

    /**
     * Removes buttons from button bar and readds them keeping their configuration. The primery
     * buttons is themed as indicated by the {@link acceptButtonType} param.
     */
    private void recreateButtons(@DualControlLayout.ButtonType int acceptButtonType) {
        mButtonBar.removeAllViews();
        Button oldButton = mAcceptButton;

        mAcceptButton =
                DualControlLayout.createButtonForLayout(
                        getContext(),
                        acceptButtonType,
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
