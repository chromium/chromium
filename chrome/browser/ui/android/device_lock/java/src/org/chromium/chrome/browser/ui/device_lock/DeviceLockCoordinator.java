// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.device_reauth.DeviceAuthRequester;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator handles the creation, update, and interaction of the device lock UI.
 */
public class DeviceLockCoordinator {
    /** Delegate for device lock MVC. */
    public interface Delegate {
        /**
         * Logic for the delegate to attach the device lock view to its own UI.
         *
         * @param view - The device lock root View.
         */
        void setView(View view);

        /**
         * The device has a device lock set and the user has chosen to continue.
         */
        void onDeviceLockReady();

        /**
         * The user has decided to dismiss the dialog without setting a device lock.
         */
        void onDeviceLockRefused();
    }

    private final DeviceLockMediator mMediator;
    private final DeviceLockView mView;
    private final WindowAndroid mWindowAndroid;
    private final ReauthenticatorBridge mDeviceLockAuthenticatorBridge;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Constructs a coordinator for the Device Lock page.
     *
     * @param inSignInFlow - Whether the existing flow is related to account sign-in.
     * @param delegate - The delegate invoked to interact with classes outside the module.
     * @param windowAndroid - Used to launch Intents with callbacks.
     * @param context - The activity context.
     */
    public DeviceLockCoordinator(
            boolean inSignInFlow, Delegate delegate, WindowAndroid windowAndroid, Context context) {
        this(inSignInFlow, delegate, windowAndroid, context,
                ReauthenticatorBridge.create(DeviceAuthRequester.DEVICE_LOCK_PAGE));
    }

    protected DeviceLockCoordinator(boolean inSignInFlow, Delegate delegate,
            WindowAndroid windowAndroid, Context context,
            ReauthenticatorBridge deviceLockAuthenticatorBridge) {
        mView = DeviceLockView.create(LayoutInflater.from(context));
        mWindowAndroid = windowAndroid;
        mDeviceLockAuthenticatorBridge = deviceLockAuthenticatorBridge;
        mMediator = new DeviceLockMediator(
                inSignInFlow, delegate, mWindowAndroid, mDeviceLockAuthenticatorBridge, context);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, DeviceLockViewBinder::bind);
        delegate.setView(mView);
    }

    /**
     * Releases the resources used by the coordinator.
     */
    public void destroy() {
        mPropertyModelChangeProcessor.destroy();
    }
}
