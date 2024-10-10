// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.ContentResolver;
import android.net.Uri;

import androidx.activity.OnBackPressedCallback;

import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.ui.base.PhotoPicker;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * UI for the photo chooser that shows on the Android platform as a result of &lt;input type=file
 * accept=image &gt; form element.
 */
public class PhotoPickerDialog extends FullscreenAlertDialog
        implements PhotoPickerToolbar.PhotoPickerToolbarDelegate, PhotoPicker {

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

        /** The constructor, supplying the {@link PhotoPickerListener} object to encapsulate. */
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

        @Override
        public void onPhotoPickerDismissed() {
            mListener.onPhotoPickerDismissed();
        }

        /** Returns whether the user picked an external intent to launch. */
        public boolean externalIntentSelected() {
            return mExternalIntentSelected;
        }
    }

    /**
     * The PhotoPickerDialog constructor.
     *
     * @param windowAndroid The window of the hosting Activity.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     * @param listener The listener object that gets notified when an action is taken.
     * @param multiSelectionAllowed Whether the photo picker should allow multiple items to be
     *     selected.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public PhotoPickerDialog(
            WindowAndroid windowAndroid,
            ContentResolver contentResolver,
            PhotoPickerListener listener,
            boolean multiSelectionAllowed,
            List<String> mimeTypes) {
        super(windowAndroid.getContext().get());

        mListenerWrapper = new PhotoPickerListenerWrapper(listener);

        // Initialize the main content view.
        mCategoryView =
                new PickerCategoryView(windowAndroid, contentResolver, multiSelectionAllowed, this);
        mCategoryView.initialize(this, mListenerWrapper, mimeTypes);
        setView(mCategoryView);
        getOnBackPressedDispatcher()
                .addCallback(
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                // Pressing Back when a video is playing, should only end the video
                                // playback.
                                boolean videoWasStopped = mCategoryView.closeVideoPlayer();
                                if (!videoWasStopped) {
                                    setEnabled(false);
                                    getOnBackPressedDispatcher().onBackPressed();
                                }
                            }
                        });
    }

    @Override
    public void dismiss() {
        if (!mListenerWrapper.externalIntentSelected() || mDoneWaitingForExternalIntent) {
            super.dismiss();
            mCategoryView.onDialogDismissed();
            mListenerWrapper.onPhotoPickerDismissed();
        }
    }

    /** Cancels the dialog in response to a back navigation. */
    @Override
    public void onNavigationBackCallback() {
        cancel();
    }

    // PhotoPicker:

    @Override
    public void onExternalIntentCompleted() {
        mDoneWaitingForExternalIntent = true;
        dismiss();
    }

    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
