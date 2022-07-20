// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.view.View;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QR Code Permission UI.
 */
public class AssistantQrCodePermissionCoordinator {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AssistantQrCodePermissionType mRequiredPermission;
    private final AssistantQrCodePermissionModel mPermissionModel;
    private final AssistantQrCodePermissionView mPermissionView;
    private AssistantQrCodePermissionBinder.ViewHolder mViewHolder;

    /**
     * The AssistantQrCodePermissionCoordinator constructor.
     */
    public AssistantQrCodePermissionCoordinator(Context context, WindowAndroid windowAndroid,
            AssistantQrCodePermissionModel permissionModel,
            AssistantQrCodePermissionType requiredPermission,
            AssistantQrCodePermissionCallback permissionCallback) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPermissionModel = permissionModel;
        mRequiredPermission = requiredPermission;

        mPermissionView = new AssistantQrCodePermissionView(
                context, requiredPermission, new AssistantQrCodePermissionView.Delegate() {
                    @Override
                    public void promptForPermission() {
                        AssistantQrCodePermissionUtils.promptForPermission(
                                windowAndroid, mRequiredPermission, permissionModel);
                    }

                    @Override
                    public void openSettings() {
                        Intent openSettingsIntent = getAppInfoIntent(mContext.getPackageName());
                        ((Activity) mContext).startActivity(openSettingsIntent);
                    }
                }, permissionCallback);

        mViewHolder = new AssistantQrCodePermissionBinder.ViewHolder(mPermissionView);
        PropertyModelChangeProcessor.create(
                mPermissionModel, mViewHolder, new AssistantQrCodePermissionBinder());
    }

    public View getView() {
        return mPermissionView.getRootView();
    }

    /** Updates the permission settings with the latest values. */
    public void updatePermissionSettings() {
        mPermissionModel.set(AssistantQrCodePermissionModel.HAS_PERMISSION,
                AssistantQrCodePermissionUtils.hasPermission(mContext, mRequiredPermission));
        mPermissionModel.set(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION,
                AssistantQrCodePermissionUtils.canPromptForPermission(
                        mWindowAndroid, mRequiredPermission));
    }

    /**
     * Returns an Intent to show the App Info page for the current app.
     */
    private Intent getAppInfoIntent(String packageName) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(new Uri.Builder().scheme("package").opaquePart(packageName).build());
        return intent;
    }

    public void destroy() {
        // Clean up view holder.
        mViewHolder = null;
    }
}
