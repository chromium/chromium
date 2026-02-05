// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.ArrayList;
import java.util.List;

/**
 * Singleton instance which controls the display of modal permission dialogs. This class is lazily
 * initiated when getInstance() is first called.
 *
 * <p>Unlike permission infobars, which stack on top of each other, only one permission dialog may
 * be visible on the screen at once. Any additional request for a modal permissions dialog is
 * queued, and will be displayed once the user responds to the current dialog.
 */
@NullMarked
public class PermissionDialogController {
    /** Interface for a class that wants to receive updates from this controller. */
    public interface Observer {
        /**
         * Notifies the observer that the user has just completed a permissions prompt.
         *
         * @param window The {@link WindowAndroid} for the prompt that just finished.
         * @param permissions An array of ContentSettingsType, indicating the last dialog
         *     permissions.
         * @param result A ContentSetting type, indicating the last dialog result.
         */
        void onDialogResult(
                WindowAndroid window,
                @ContentSettingsType.EnumType int[] permissions,
                @ContentSetting int result);

        /**
         * Notifies the observer that a quiet permission icon should be shown.
         *
         * @param window The {@link WindowAndroid} where the icon should be shown.
         */
        default void showPermissionClapperQuietIcon(WindowAndroid window) {}

        /**
         * Notifies the observer that a quiet permission icon should be dismissed.
         *
         * @param window The {@link WindowAndroid} where the icon should be dismissed.
         */
        default void dismissPermissionClapperQuietIcon(WindowAndroid window) {}
    }

    private class PermissionDialogCoordinatorDelegate
            implements PermissionDialogCoordinator.Delegate {
        @Override
        public void onPermissionDialogResult(@ContentSetting int result) {
            notifyObservers(result);
        }

        @Override
        public void onPermissionDialogEnded() {
            if (mCoordinator != null) {
                mCoordinator.destroy();
                mCoordinator = null;
            }
            mDialogDelegate = null;
            scheduleDisplay();
        }
    }

    private final ObserverList<Observer> mObservers;

    private @Nullable PermissionDialogDelegate mDialogDelegate;
    private @Nullable PermissionDialogCoordinator mCoordinator;

    // As the PermissionRequestManager handles queueing for a tab and only shows prompts for active
    // tabs, we typically only have one request. This class only handles multiple requests at once
    // when either:
    // 1) Multiple open windows request permissions due to Android split-screen
    // 2) A tab navigates or is closed while the Android permission request is open, and the
    // subsequent page requests a permission
    private final List<PermissionDialogDelegate> mRequestQueue;

    // Static holder to ensure safe initialization of the singleton instance.
    private static class Holder {
        @SuppressLint("StaticFieldLeak")
        private static final PermissionDialogController sInstance =
                new PermissionDialogController();
    }

    public static PermissionDialogController getInstance() {
        return Holder.sInstance;
    }

    private PermissionDialogController() {
        mRequestQueue = new ArrayList<>();
        mObservers = new ObserverList<>();
    }

    /**
     * Called by native code to create a modal permission dialog. The PermissionDialogController
     * will decide whether the dialog needs to be queued (because another dialog is on the screen)
     * or whether it can be shown immediately.
     */
    @CalledByNative
    private static void createDialog(PermissionDialogDelegate delegate) {
        PermissionDialogController.getInstance().queueDialog(delegate);
    }

    /**
     * Called by native code to show the quiet permission icon.
     *
     * @param window The {@link WindowAndroid} where the icon should be shown.
     */
    @CalledByNative
    public static void showPermissionClapperQuietIcon(WindowAndroid window) {
        PermissionDialogController.getInstance().notifyShowPermissionClapperQuietIcon(window);
    }

    /**
     * Called by native code to dismiss the quiet permission icon.
     *
     * @param window The {@link WindowAndroid} where the icon should be dismissed.
     */
    @CalledByNative
    public static void dismissPermissionClapperQuietIcon(WindowAndroid window) {
        PermissionDialogController.getInstance().notifyDismissPermissionClapperQuietIcon(window);
    }

    /**
     * Called by native code to show the loud permission icon.
     *
     * <p>This is part of the clapper loud permission prompt flow for notifications and triggers the
     * omnibox icon after a permission request was decided via the message ui.
     *
     * @param window The {@link WindowAndroid} where the icon should be shown.
     * @param result A ContentSetting type, indicating the result.
     */
    @CalledByNative
    public static void showLoudClapperDialogResultIcon(
            WindowAndroid window, @ContentSetting int result) {
        PermissionDialogController.getInstance()
                .notifyObservers(window, new int[] {ContentSettingsType.NOTIFICATIONS}, result);
    }

    /**
     * Notifies observers of a permission result.
     *
     * @param window The {@link WindowAndroid} for the prompt that just finished.
     * @param permissions An array of ContentSettingsType, indicating the permissions.
     * @param result A ContentSetting type, indicating the result.
     */
    public void notifyObservers(
            WindowAndroid window,
            @ContentSettingsType.EnumType int[] permissions,
            @ContentSetting int result) {
        for (Observer obs : mObservers) {
            obs.onDialogResult(window, permissions, result);
        }
    }

    /**
     * Notifies observers that a quiet permission icon should be shown.
     *
     * @param window The {@link WindowAndroid} where the icon should be shown.
     */
    public void notifyShowPermissionClapperQuietIcon(WindowAndroid window) {
        for (Observer obs : mObservers) {
            obs.showPermissionClapperQuietIcon(window);
        }
    }

    /**
     * Notifies observers that a quiet permission icon should be dismissed.
     *
     * @param window The {@link WindowAndroid} where the icon should be dismissed.
     */
    public void notifyDismissPermissionClapperQuietIcon(WindowAndroid window) {
        for (Observer obs : mObservers) {
            obs.dismissPermissionClapperQuietIcon(window);
        }
    }

    /**
     * @param observer An observer to be notified of changes.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Update the dialog for the given native delegate, if the delegate points to the current
     * displaying dialog. This may hide the current dialog and show OS prompt instead
     *
     * @param delegate The wrapper for the native-side permission delegate.
     */
    public void updateDialog(PermissionDialogDelegate delegate) {
        if (mDialogDelegate == delegate) {
            assumeNonNull(mCoordinator).updateDialog();
        }
    }

    /**
     * Queues a modal permission dialog for display. If there are currently no dialogs on screen, it
     * will be displayed immediately. Otherwise, it will be displayed as soon as the user responds
     * to the current dialog.
     *
     * @param delegate The wrapper for the native-side permission delegate.
     */
    private void queueDialog(PermissionDialogDelegate delegate) {
        mRequestQueue.add(delegate);
        delegate.setDialogController(this);
        scheduleDisplay();
    }

    private void scheduleDisplay() {
        if (mCoordinator == null && !mRequestQueue.isEmpty()) dequeueDialog();
    }

    /** Shows the dialog asking the user for a web API permission. */
    private void dequeueDialog() {
        assert mCoordinator == null;
        mDialogDelegate = mRequestQueue.remove(0);
        Context context = mDialogDelegate.getWindow().getContext().get();

        // It's possible for the context to be null if we reach here just after the user
        // backgrounds the browser and cleanup has happened. In that case, we can't show a prompt,
        // so act as though the user dismissed it.
        if (context == null) {
            notifyObservers(ContentSetting.DEFAULT);
            mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_NO_CONTEXT);
            return;
        }

        mCoordinator = new PermissionDialogCoordinator(new PermissionDialogCoordinatorDelegate());
        if (!mCoordinator.showDialog(mDialogDelegate)) {
            scheduleDisplay();
        }
    }

    public void dismissFromNative(PermissionDialogDelegate delegate) {
        if (mDialogDelegate != null && mDialogDelegate != delegate) {
            assert mRequestQueue.contains(delegate);
            mRequestQueue.remove(delegate);
        } else {
            assumeNonNull(mCoordinator).dismissFromNative();
        }
        delegate.destroy();
    }

    public void dismissByCloseButton(PermissionDialogDelegate delegate) {
        if (mDialogDelegate != null && mDialogDelegate != delegate) {
            assert mRequestQueue.contains(delegate);
            mRequestQueue.remove(delegate);
        } else {
            assumeNonNull(mCoordinator).dismissByCloseButton();
        }
    }

    public void notifyObservers(@ContentSetting int result) {
        if (result != ContentSetting.DEFAULT) {
            assert mDialogDelegate != null;
            WindowAndroid currentWindow = mDialogDelegate.getWindow();
            for (Observer obs : mObservers) {
                obs.onDialogResult(
                        currentWindow, mDialogDelegate.getContentSettingsTypes().clone(), result);
            }
        }
    }

    public void notifyPermissionAllowed(PermissionDialogDelegate delegate) {
        if (mDialogDelegate == delegate) {
            notifyObservers(ContentSetting.ALLOW);
        }
    }

    /**
     * Update the icon of the current custom view for the given bit map.
     *
     * @param delegate The wrapper for the native-side permission delegate.
     * @param icon The bitmap icon to display on the custom view.
     */
    public void updateIcon(PermissionDialogDelegate delegate, Bitmap icon) {
        if (mDialogDelegate == delegate) {
            assumeNonNull(mCoordinator).updateIcon(icon);
        }
    }

    /**
     * Get size of icon showing on the custom view.
     *
     * @param delegate The wrapper for the native-side permission delegate.
     * @return icon The size of icon displayed on the custom view.
     */
    public int getIconSizeInPx(PermissionDialogDelegate delegate) {
        return mDialogDelegate == delegate ? assumeNonNull(mCoordinator).getIconSizeInPx() : 0;
    }

    public boolean isDialogShownForTest() {
        return mDialogDelegate != null;
    }

    public void clickButtonForTest(@ModalDialogProperties.ButtonType int buttonType) {
        assumeNonNull(mCoordinator).clickButtonForTest(buttonType); // IN-TEST
    }
}
