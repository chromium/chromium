// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.core.content.ContextCompat;

import org.chromium.components.autofill_assistant.guided_browsing.LayoutUtils;
import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Manages the Android View representing the QR Code permission.
 */
public class AssistantQrCodePermissionView {
    /** Interface used to delegate various user interactions to the coordinator. */
    public interface Delegate {
        /** Prompts the user for the permission */
        void promptForPermission();
        /** Open settings **/
        void openSettings();
    }

    private final Context mContext;
    private final AssistantQrCodePermissionView.Delegate mViewDelegate;
    private final AssistantQrCodePermissionCallback mPermissionCallback;

    private final View mPermissionView;
    private final TextView mPermissionTextView;
    private final ButtonCompat mPermissionButton;

    private boolean mHasPermission;
    private boolean mCanPromptForPermission;
    private boolean mHasPromptedForPermissionOnce;

    /**
     * The AssistantQrCodePermissionView constructor.
     */
    @SuppressWarnings("DiscouragedApi")
    public AssistantQrCodePermissionView(Context context, AssistantQrCodePermissionType permission,
            AssistantQrCodePermissionView.Delegate delegate,
            AssistantQrCodePermissionCallback permissionCallback) {
        mContext = context;
        mViewDelegate = delegate;
        mPermissionCallback = permissionCallback;
        mPermissionView = LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_qr_code_permission_layout,
                /* root= */ null);

        mPermissionTextView = mPermissionView.findViewById(R.id.permission_text);
        mPermissionButton = mPermissionView.findViewById(R.id.permission_button);

        mHasPromptedForPermissionOnce = true;

        // Updating permission view image based on the permission type.
        ChromeImageView permissionImageView = mPermissionView.findViewById(R.id.permission_image);
        int permissionImageResouce = mContext.getResources().getIdentifier(
                permission.getAndroidPermissionImage(), "drawable", mContext.getPackageName());
        permissionImageView.setImageDrawable(
                ContextCompat.getDrawable(mContext, permissionImageResouce));

        updatePermissionButtonBehaviour();
    }

    public View getRootView() {
        return mPermissionView;
    }

    TextView getPermissionTextView() {
        return mPermissionTextView;
    }

    ButtonCompat getPermissionButton() {
        return mPermissionButton;
    }

    /**
     * Updates the permission button behaviour based on the value of |canPromptForPermission|.
     */
    private void updatePermissionButtonBehaviour() {
        mPermissionButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mCanPromptForPermission) {
                    mViewDelegate.promptForPermission();
                } else {
                    mViewDelegate.openSettings();
                }
            }
        });
    }

    /**
     * Updates the state of the view based on the updated value of |hasPermission|.
     *
     * @param hasPermission Indicates whether permissions were granted.
     */
    void onHasPermissionChanged(Boolean hasPermission) {
        // No change, nothing to do here
        if (mHasPermission && hasPermission) {
            return;
        }
        mHasPermission = hasPermission;
        // We do not need to change button behaviour because some other view will open when
        // permission is granted.
        mPermissionCallback.onPermissionsChanged(mHasPermission);
    }

    /**
     * Updates the state of the view based on the updated value of |canPromptForPermission|.
     *
     * @param canPromptForPermission Indicates whether the user can be prompted for permission
     */
    void canPromptForPermissionChanged(Boolean canPromptForPermission) {
        mCanPromptForPermission = canPromptForPermission;

        // When canPrompt value changes, the Permission view changes. We then ask user to open
        // settings, hence attach relevant listener.
        updatePermissionButtonBehaviour();
    }

    /**
     * Prompt the permission once. This should be done only if the required permission is not
     * granted and we can prompt the permission.
     *
     * Please ensure that both |mCanPromptForPermission| and |mHasPermission| are up to date. We
     * only allow once to prompt the permission.
     */
    void maybePromptForPermissionOnce() {
        if (mCanPromptForPermission && !mHasPermission && mHasPromptedForPermissionOnce) {
            // Open the permission prompter to ask for the permissions.
            mViewDelegate.promptForPermission();
            mHasPromptedForPermissionOnce = !mHasPromptedForPermissionOnce;
        }
    }
}
