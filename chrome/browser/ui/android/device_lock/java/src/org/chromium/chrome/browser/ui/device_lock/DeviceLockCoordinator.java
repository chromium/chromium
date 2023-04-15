// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator handles the creation, update, and interaction of the device lock UI.
 */
public class DeviceLockCoordinator {
    /** Delegate for device lock MVC. */
    public interface Delegate {
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
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public DeviceLockCoordinator(boolean inSignInFlow, Delegate delegate, Context context) {
        mView = DeviceLockView.create(LayoutInflater.from(context));
        mMediator = new DeviceLockMediator(inSignInFlow, delegate, context);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, DeviceLockViewBinder::bind);
    }

    /**
     * Releases the resources used by the coordinator.
     */
    public void destroy() {
        mPropertyModelChangeProcessor.destroy();
    }
}
