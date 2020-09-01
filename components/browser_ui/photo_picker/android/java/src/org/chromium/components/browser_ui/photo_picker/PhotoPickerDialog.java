// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.app.Activity;
import android.content.ContentResolver;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.vr.VrModeProvider;

import java.util.List;

/**
 * UI for the photo chooser that shows on the Android platform as a result of
 * &lt;input type=file accept=image &gt; form element.
 */
public class PhotoPickerDialog
        extends AlertDialog implements PhotoPickerToolbar.PhotoPickerToolbarDelegate {
    // Our window.
    private WindowAndroid mWindowAndroid;

    // The category we're showing photos for.
    private PickerCategoryView mCategoryView;

    // A wrapper around the listener object, watching to see if an external intent is launched.
    private PhotoPickerListenerWrapper mListenerWrapper;

    // Whether the wait for an external intent launch is over.
    private boolean mDoneWaitingForExternalIntent;

    /**
     * A wrapper around {@link PhotoPickerListener} that listens for external intents being
     * launched.
     */
    private static class PhotoPickerListenerWrapper implements PhotoPickerListener {
        // The {@link PhotoPickerListener} to forward the events to.
        PhotoPickerListener mListener;

        // Whether the user selected to launch an external intent.
        private boolean mExternalIntentSelected;

        /**
         * The constructor, supplying the {@link PhotoPickerListener} object to encapsulate.
         */
        public PhotoPickerListenerWrapper(PhotoPickerListener listener) {
            mListener = listener;
        }

        // PhotoPickerListener:
        @Override
        public void onPhotoPickerUserAction(@PhotoPickerAction int action, Uri[] photos) {
            mExternalIntentSelected = false;
            if (action == PhotoPickerAction.LAUNCH_GALLERY
                    || action == PhotoPickerAction.LAUNCH_CAMERA) {
                mExternalIntentSelected = true;
            }

            mListener.onPhotoPickerUserAction(action, photos);
        }

        /**
         * Returns whether the user picked an external intent to launch.
         */
        public boolean externalIntentSelected() {
            return mExternalIntentSelected;
        }
    }

    /**
     * The PhotoPickerDialog constructor.
     * @param windowAndroid The window of the hosting Activity.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     * @param listener The listener object that gets notified when an action is taken.
     * @param multiSelectionAllowed Whether the photo picker should allow multiple items to be
     *                              selected.
     * @param mimeTypes A list of mime types to show in the dialog.
     * @param vrModeProvider Used to query VR mode state.
     */
    public PhotoPickerDialog(WindowAndroid windowAndroid, ContentResolver contentResolver,
            PhotoPickerListener listener, boolean multiSelectionAllowed, List<String> mimeTypes,
            VrModeProvider vrModeProvider) {
        super(windowAndroid.getContext().get(), R.style.Theme_Chromium_Fullscreen);

        mWindowAndroid = windowAndroid;
        mListenerWrapper = new PhotoPickerListenerWrapper(listener);

        // Initialize the main content view.
        mCategoryView = new PickerCategoryView(
                windowAndroid, contentResolver, multiSelectionAllowed, this, vrModeProvider);
        mCategoryView.initialize(this, mListenerWrapper, mimeTypes);
        setView(mCategoryView);
    }

    @Override
    public void onBackPressed() {
        // Pressing Back when a video is playing, should only end the video playback.
        boolean videoWasStopped = mCategoryView.closeVideoPlayer();
        if (videoWasStopped) {
            return;
        } else {
            super.onBackPressed();
        }
    }

    @Override
    public void dismiss() {
        if (!mListenerWrapper.externalIntentSelected() || mDoneWaitingForExternalIntent) {
            super.dismiss();
            mCategoryView.onDialogDismissed();
        } else {
            ApplicationStatus.registerStateListenerForActivity(new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    // When an external intent, such as the Camera intent, is launched, this
                    // listener will first receive the PAUSED event. Normally, STOPPED is the next
                    // event, as the Camera intent appears. But if the user presses Back quickly
                    // after the PAUSED event, the STOPPED event will not arrive, and this listener
                    // gets RESUMED instead. However, we are already in teardown mode, so the
                    // safe thing to do is to close the dialog.
                    if (newState == ActivityState.STOPPED || newState == ActivityState.RESUMED) {
                        mDoneWaitingForExternalIntent = true;
                        ApplicationStatus.unregisterActivityStateListener(this);
                        dismiss();
                    }
                }
            }, ContextUtils.activityFromContext(mWindowAndroid.getContext().get()));
        }
    }

    /**
     * Cancels the dialog in response to a back navigation.
     */
    @Override
    public void onNavigationBackCallback() {
        cancel();
    }

    @VisibleForTesting
    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
