// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import com.google.android.play.core.splitcompat.SplitCompat;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;

import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/** Installs Dynamic Feature Modules (DFMs). */
public class ModuleInstaller {
    private static final String TAG = "ModuleInstaller";
    private static final Map<String, List<OnFinishedListener>> sModuleNameListenerMap =
            new HashMap<>();
    private static SplitInstallManager sManager;
    private static SplitInstallStateUpdatedListener sUpdateListener;

    /** Listener for when a module install has finished. */
    public interface OnFinishedListener {
        /**
         * Called when the install has finished.
         *
         * @param success True if the module was installed successfully.
         */
        void onFinished(boolean success);
    }

    /** Needs to be called before trying to access a module. */
    public static void init() {
        // SplitCompat.install may copy modules into Chrome's internal folder or clean them up.
        try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
            SplitCompat.install(ContextUtils.getApplicationContext());
        }
    }

    /**
     * Requests the install of a module. The install will be performed asynchronously.
     *
     * @param moduleName Name of the module as defined in GN.
     * @param onFinishedListener Listener to be called once installation is finished.
     */
    public static void install(String moduleName, OnFinishedListener onFinishedListener) {
        ThreadUtils.assertOnUiThread();

        if (!sModuleNameListenerMap.containsKey(moduleName)) {
            sModuleNameListenerMap.put(moduleName, new LinkedList<>());
        }
        List<OnFinishedListener> onFinishedListeners = sModuleNameListenerMap.get(moduleName);
        onFinishedListeners.add(onFinishedListener);
        if (onFinishedListeners.size() > 1) {
            // Request is already running.
            return;
        }

        SplitInstallRequest request =
                SplitInstallRequest.newBuilder().addModule(moduleName).build();
        getManager().startInstall(request).addOnFailureListener(exception -> {
            Log.e(TAG, "Failed to request module '" + moduleName + "': " + exception);
            onFinished(false, Arrays.asList(moduleName));
        });
    }

    private static void onFinished(boolean success, List<String> moduleNames) {
        ThreadUtils.assertOnUiThread();

        for (String moduleName : moduleNames) {
            List<OnFinishedListener> onFinishedListeners = sModuleNameListenerMap.get(moduleName);
            if (onFinishedListeners == null) continue;

            for (OnFinishedListener listener : onFinishedListeners) {
                listener.onFinished(success);
            }
            sModuleNameListenerMap.remove(moduleName);
        }

        if (sModuleNameListenerMap.isEmpty()) {
            sManager.unregisterListener(sUpdateListener);
            sUpdateListener = null;
            sManager = null;
        }
    }

    private static SplitInstallManager getManager() {
        ThreadUtils.assertOnUiThread();

        if (sManager == null) {
            sManager = SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());
            sUpdateListener = (state) -> {
                switch (state.status()) {
                    case SplitInstallSessionStatus.INSTALLED:
                        onFinished(true, state.moduleNames());
                        break;
                    case SplitInstallSessionStatus.CANCELED:
                    case SplitInstallSessionStatus.FAILED:
                        Log.e(TAG,
                                "Failed to install modules '" + state.moduleNames()
                                        + "': " + state.status());
                        onFinished(false, state.moduleNames());
                        break;
                }
            };
            sManager.registerListener(sUpdateListener);
        }
        return sManager;
    }

    private ModuleInstaller() {}
}
