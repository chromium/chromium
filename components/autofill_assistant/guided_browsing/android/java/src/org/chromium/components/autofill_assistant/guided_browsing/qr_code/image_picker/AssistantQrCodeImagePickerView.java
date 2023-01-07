// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker;

import android.content.Context;
import android.content.Intent;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.components.autofill_assistant.guided_browsing.LayoutUtils;
import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Manages the Android View representing the QR Code Image Picker panel. It manages between showing
 * the permission view and triggering android image picker based on the permissions.
 */
class AssistantQrCodeImagePickerView {
    /** Interface used to delegate various user interactions to the coordinator. */
    public interface Delegate {
        /** Called when user cancels the QR Code Scanning */
        void onScanCancelled();
    }

    private static final String IMAGE_TYPE = "image";
    private final Context mContext;
    private final AssistantQrCodeImagePickerView.Delegate mViewDelegate;
    private final IntentCallback mImagePickerCallback;
    private final WindowAndroid mWindowAndroid;

    private final View mRootView;
    // Will be used to update the main body of the view based on state.
    private final FrameLayout mBodyView;
    private final View mReadImagesPermissionView;

    private boolean mHasReadImagesPermission;
    private boolean mIsOnForeground;

    /**
     * The AssistantQrCodeImagePickerView constructor.
     */
    AssistantQrCodeImagePickerView(Context context, WindowAndroid windowAndroid,
            View readImagesPermissionView, IntentCallback imagePickerCallback,
            AssistantQrCodeImagePickerView.Delegate delegate) {
        mContext = context;
        mViewDelegate = delegate;
        mWindowAndroid = windowAndroid;
        mImagePickerCallback = imagePickerCallback;

        mBodyView = new FrameLayout(context);
        mRootView = createRootView();
        mReadImagesPermissionView = readImagesPermissionView;
    }

    View getRootView() {
        return mRootView;
    }

    /**
     * Updates the state of the view based on the updated value of |isOnForeground|.
     *
     * @param isOnForeground Indicates whether this component UI is currently on foreground.
     */
    void onForegroundChanged(Boolean isOnForeground) {
        mIsOnForeground = isOnForeground;
        if (mIsOnForeground) {
            displayUpdatedView();
        }
    }

    /**
     * Updates the state of the view based on the updated value of |hasReadImagesPermission|.
     *
     * @param hasReadImagesPermission Indicates whether this component UI has permission to read
     *        images or not.
     */
    void onPermissionsChanged(boolean hasReadImagesPermission) {
        mHasReadImagesPermission = hasReadImagesPermission;
        displayUpdatedView();
    }

    /**
     * Creates the root view for the QR Code Image Picker Dialog.
     */
    private View createRootView() {
        View dialogView = LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_qr_code_dialog, /* root= */ null);

        ChromeImageButton closeButton = dialogView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> { mViewDelegate.onScanCancelled(); });

        FrameLayout layout = dialogView.findViewById(R.id.qr_code_view);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        layout.addView(mBodyView, params);

        return dialogView;
    }

    /**
     * Update the view based on various state parameters:
     * - app is in the foreground
     * - user has given permission to read images.
     * - user can be prompted for permission to read images.
     */
    private void displayUpdatedView() {
        // The scan tab is not in the foreground so don't do any rendering.
        if (!mIsOnForeground) {
            return;
        }

        if (mHasReadImagesPermission) {
            openAndroidImagePicker();
        } else {
            displayReadImagesPermissionView();
        }
    }

    /**
     * Opens the android image picker. Caller should check that we already have the read images
     * permissions.
     */
    private void openAndroidImagePicker() {
        mBodyView.removeAllViews();
        Intent openAndroidImagePickerIntent = new Intent(Intent.ACTION_PICK);
        openAndroidImagePickerIntent.setType(IMAGE_TYPE + "/*");
        mWindowAndroid.showIntent(
                openAndroidImagePickerIntent, mImagePickerCallback, R.string.low_memory_error);
    }

    /**
     * Displays the permission dialog. Caller should check that the user can be prompted and hasn't
     * permanently denied permission.
     */
    private void displayReadImagesPermissionView() {
        mBodyView.removeAllViews();
        mBodyView.addView(mReadImagesPermissionView);
    }
}
