// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.DualControlLayout.ButtonType;

/**
 * View shown to a user who has removed the device lock to inform them that their private data will
 * be deleted from Chrome if they continue, and prompts them to create a new device lock otherwise.
 */
@NullMarked
public class MissingDeviceLockView extends LinearLayout {
    private TextView mTitle;
    private TextView mDescription;
    private CheckBox mCheckBox;
    private DualControlLayout mButtonBar;
    private Button mContinueButton;
    private Button mCreateDeviceLockButton;

    public MissingDeviceLockView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public static MissingDeviceLockView create(LayoutInflater inflater) {
        MissingDeviceLockView view =
                (MissingDeviceLockView)
                        inflater.inflate(R.layout.missing_device_lock_view, null, false);
        view.setClipToOutline(true);
        return view;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.missing_device_lock_title);
        mDescription = findViewById(R.id.missing_device_lock_description);
        mCheckBox = findViewById(R.id.missing_device_lock_remove_local_data);

        mCreateDeviceLockButton =
                DualControlLayout.createButtonForLayout(
                        getContext(),
                        ButtonType.SECONDARY_TEXT,
                        getResources().getString(R.string.device_lock_create_lock_button),
                        null);
        mCreateDeviceLockButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mContinueButton =
                DualControlLayout.createButtonForLayout(
                        getContext(),
                        ButtonType.PRIMARY_FILLED,
                        getResources().getString(R.string.delete_and_continue),
                        null);
        mContinueButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mButtonBar = findViewById(R.id.dual_control_button_bar);
        mButtonBar.addView(mContinueButton);
        mButtonBar.addView(mCreateDeviceLockButton);

        ImageView illustration = findViewById(R.id.missing_device_lock_illustration);
        MarginLayoutParams illustrationParams = (MarginLayoutParams) illustration.getLayoutParams();
        int illustrationTopMargin;

        illustration.setBackgroundColor(Color.TRANSPARENT);
        illustrationParams.height =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.device_lock_dialog_illustration_height);
        illustrationTopMargin =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.device_lock_dialog_illustration_top_margin);
        mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.STACK);
        DeviceLockUtils.updateDialogSubviewMargins(mButtonBar);
        DeviceLockUtils.updateDialogSubviewMargins(mTitle);
        DeviceLockUtils.updateDialogSubviewMargins(mDescription);
        DeviceLockUtils.updateDialogSubviewMargins(mCheckBox);
        illustrationParams.setMargins(
                illustrationParams.leftMargin,
                illustrationTopMargin,
                illustrationParams.rightMargin,
                illustrationParams.bottomMargin);
        illustration.setLayoutParams(illustrationParams);
    }

    TextView getTitle() {
        return mTitle;
    }

    TextView getDescription() {
        return mDescription;
    }

    CheckBox getCheckbox() {
        return mCheckBox;
    }

    TextView getContinueButton() {
        return mContinueButton;
    }

    TextView getCreateDeviceLockButton() {
        return mCreateDeviceLockButton;
    }
}
