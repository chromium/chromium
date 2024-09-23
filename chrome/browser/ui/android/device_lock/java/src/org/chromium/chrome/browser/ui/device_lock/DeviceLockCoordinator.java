// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.accounts.Account;
import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator handles the creation, update, and interaction of the device lock UI. */
public class DeviceLockCoordinator {
    /** Delegate for device lock MVC. */
    public interface Delegate {
        /**
         * Logic for the delegate to attach the device lock view to its own UI.
         *
         * @param view - The device lock root View.
         */
        void setView(View view);

        /** The device has a device lock set and the user has chosen to continue. */
        void onDeviceLockReady();

        /** The user has decided to dismiss the dialog without setting a device lock. */
        void onDeviceLockRefused();

        /** Returns which source the user accessed the device lock UI from. */
        @DeviceLockActivityLauncher.Source
        String getSource();
    }

    private final DeviceLockMediator mMediator;
    private final DeviceLockView mView;
    private final WindowAndroid mWindowAndroid;
    private final @Nullable ReauthenticatorBridge mDeviceLockAuthenticatorBridge;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Constructs a coordinator for the Device Lock page.
     *
     * @param delegate The delegate invoked to interact with classes outside the module.
     * @param windowAndroid Used to launch Intents with callbacks.
     * @param profile The Profile associated with this session.
     * @param activity The activity hosting this page.
     * @param account The account that will be used for the reauthentication challenge, or null if
     *     reauthentication is not needed.
     */
    public DeviceLockCoordinator(
            Delegate delegate,
            WindowAndroid windowAndroid,
            Profile profile,
            Activity activity,
            @Nullable Account account) {
        this(
                delegate,
                windowAndroid,
                createDeviceLockAuthenticatorBridge(activity, profile),
                activity,
                account);
    }

    /**
     * Constructs a coordinator for the Device Lock page.
     *
     * @param delegate The delegate invoked to interact with classes outside the module.
     * @param windowAndroid Used to launch Intents with callbacks.
     * @param deviceLockAuthenticatorBridge The {@link ReauthenticatorBridge} used to confirm device
     *     lock credentials.
     * @param activity The activity hosting this page.
     * @param account The account that will be used for the reauthentication challenge, or null if
     *     reauthentication is not needed.
     */
    public DeviceLockCoordinator(
            Delegate delegate,
            WindowAndroid windowAndroid,
            @Nullable ReauthenticatorBridge deviceLockAuthenticatorBridge,
            Activity activity,
            @Nullable Account account) {
        mView = DeviceLockView.create(LayoutInflater.from(activity));
        mWindowAndroid = windowAndroid;
        mDeviceLockAuthenticatorBridge = deviceLockAuthenticatorBridge;
        mMediator =
                new DeviceLockMediator(
                        delegate,
                        mWindowAndroid,
                        mDeviceLockAuthenticatorBridge,
                        activity,
                        account);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, DeviceLockViewBinder::bind);
        delegate.setView(mView);
    }

    /** Get a {@link ReauthenticatorBridge} for the Device Lock page. */
    public static ReauthenticatorBridge createDeviceLockAuthenticatorBridge(
            Activity activity, Profile profile) {
        return ReauthenticatorBridge.create(activity, profile, DeviceAuthSource.DEVICE_LOCK_PAGE);
    }

    /** Releases the resources used by the coordinator. */
    public void destroy() {
        if (mDeviceLockAuthenticatorBridge != null) {
            mDeviceLockAuthenticatorBridge.destroy();
        }
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
        }
    }
}
