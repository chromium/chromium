// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.DualControlLayout.ButtonType;
import org.chromium.components.browser_ui.widget.MaterialProgressBar;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;

/**
 * View that displays the device lock page to users and prompts them to create one if none are
 * present on the device.
 */
@NullMarked
public class DeviceLockView extends LinearLayout {
    private MaterialProgressBar mProgressBar;
    private TextView mTitle;
    private TextView mDescription;
    private TextView mNoticeText;
    private TextViewWithCompoundDrawables mNoticeTextLegacy;
    private DualControlLayout mButtonBar;
    private Button mContinueButton;
    private Button mDismissButton;

    public DeviceLockView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public static DeviceLockView create(LayoutInflater inflater) {
        DeviceLockView view =
                (DeviceLockView) inflater.inflate(R.layout.device_lock_view, null, false);
        view.setClipToOutline(true);
        return view;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.device_lock_title);
        mProgressBar = findViewById(R.id.device_lock_linear_progress_indicator);
        mProgressBar.setIndeterminate(true);
        mDescription = findViewById(R.id.device_lock_description);
        mNoticeText = findViewById(R.id.device_lock_notice);
        mNoticeTextLegacy = findViewById(R.id.device_lock_notice_legacy);

        int buttonWidth =
                SigninFeatureMap.isEnabled(SigninFeatures.UNO_FOR_AUTO)
                        ? ViewGroup.LayoutParams.MATCH_PARENT
                        : ViewGroup.LayoutParams.WRAP_CONTENT;
        mDismissButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), ButtonType.SECONDARY_TEXT, "", null);
        mDismissButton.setLayoutParams(
                new ViewGroup.LayoutParams(buttonWidth, ViewGroup.LayoutParams.WRAP_CONTENT));

        mContinueButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), ButtonType.PRIMARY_FILLED, "", null);
        mContinueButton.setLayoutParams(
                new ViewGroup.LayoutParams(buttonWidth, ViewGroup.LayoutParams.WRAP_CONTENT));

        mButtonBar = findViewById(R.id.dual_control_button_bar);
        mButtonBar.addView(mContinueButton);
        mButtonBar.addView(mDismissButton);

        ImageView illustration = findViewById(R.id.device_lock_illustration);
        MarginLayoutParams illustrationParams = (MarginLayoutParams) illustration.getLayoutParams();
        int illustrationTopMargin;

        if (SigninFeatureMap.isEnabled(SigninFeatures.UNO_FOR_AUTO)) {
            illustration.setBackgroundColor(Color.TRANSPARENT);
            illustrationParams.height =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.device_lock_dialog_illustration_height);
            illustrationTopMargin =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(
                                    R.dimen.device_lock_dialog_illustration_top_margin);
            findViewById(R.id.device_lock_notice_container).setVisibility(View.GONE);
            mNoticeText.setVisibility(View.VISIBLE);
            mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.STACK);
            DeviceLockUtils.updateDialogSubviewMargins(mTitle);
            DeviceLockUtils.updateDialogSubviewMargins(mDescription);
            DeviceLockUtils.updateDialogSubviewMargins(mNoticeText);
            DeviceLockUtils.updateDialogSubviewMargins(mButtonBar);
        } else {
            illustration.setBackgroundColor(
                    getContext().getColor(R.color.signin_header_animation_background));
            illustrationTopMargin = 0;
            findViewById(R.id.device_lock_notice_container).setVisibility(View.VISIBLE);
            mNoticeText.setVisibility(View.GONE);
            mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.APART);
        }
        illustrationParams.setMargins(
                illustrationParams.leftMargin,
                illustrationTopMargin,
                illustrationParams.rightMargin,
                illustrationParams.bottomMargin);
        illustration.setLayoutParams(illustrationParams);
    }

    MaterialProgressBar getProgressBar() {
        return mProgressBar;
    }

    TextView getTitle() {
        return mTitle;
    }

    TextView getDescription() {
        return mDescription;
    }

    TextView getNoticeText() {
        return SigninFeatureMap.isEnabled(SigninFeatures.UNO_FOR_AUTO)
                ? mNoticeText
                : mNoticeTextLegacy;
    }

    TextView getContinueButton() {
        return mContinueButton;
    }

    TextView getDismissButton() {
        return mDismissButton;
    }
}
