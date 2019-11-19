// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import com.google.android.play.core.splitinstall.SplitInstallException;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Install engine that uses Play Core and SplitCompat to install modules.
 */
class SplitCompatEngine implements InstallEngine {
    private final SplitCompatEngineFacade mFacade;
    private final SplitInstallStateUpdatedListener mUpdateListener = getStatusUpdateListener();
    private static final Map<String, List<InstallListener>> sSessions = new HashMap<>();

    public SplitCompatEngine() {
        this(new SplitCompatEngineFacade());
    }

    public SplitCompatEngine(SplitCompatEngineFacade facade) {
        mFacade = facade;
        mFacade.initApplicationContext(this);
    }

    @Override
    public void initActivity(Activity activity) {
        mFacade.installActivity(activity);
    }

    @Override
    public boolean isInstalled(String moduleName) {
        Set<String> installedModules = mFacade.getSplitManager().getInstalledModules();
        return installedModules.contains(moduleName);
    }

    @Override
    public void installDeferred(String moduleName) {
        mFacade.getSplitManager().deferredInstall(Collections.singletonList(moduleName));
        mFacade.getLogger().logRequestDeferredStart(moduleName);
    }

    @Override
    public void install(String moduleName, InstallListener listener) {
        ThreadUtils.assertOnUiThread();

        if (sSessions.containsKey(moduleName)) {
            sSessions.get(moduleName).add(listener);
            return;
        }

        registerUpdateListener();

        sSessions.put(moduleName, new ArrayList<InstallListener>() {
            { add(listener); }
        });

        SplitInstallRequest request = mFacade.createSplitInstallRequest(moduleName);

        mFacade.getSplitManager().startInstall(request).addOnFailureListener(ex -> {
            // TODO(fredmello): look into potential issues with mixing split error code
            // with our logger codes - fix accordingly.
            mFacade.getLogger().logRequestFailure(moduleName,
                    ex instanceof SplitInstallException
                            ? ((SplitInstallException) ex).getErrorCode()
                            : mFacade.getLogger().getUnknownRequestErrorCode());

            String message = String.format(Locale.US, "Request Exception: %s", ex.getMessage());
            notifyListeners(moduleName, false);
        });

        mFacade.getLogger().logRequestStart(moduleName);
    }

    private SplitInstallStateUpdatedListener getStatusUpdateListener() {
        return state -> {
            if (state.moduleNames().size() != 1) {
                throw new UnsupportedOperationException("Only one module supported.");
            }

            int status = state.status();
            String moduleName = state.moduleNames().get(0);

            switch (status) {
                case SplitInstallSessionStatus.INSTALLED:
                    notifyListeners(moduleName, true);
                    break;
                case SplitInstallSessionStatus.FAILED:
                    notifyListeners(moduleName, false);
                    mFacade.getLogger().logStatusFailure(moduleName, state.errorCode());
                    break;
            }

            mFacade.getLogger().logStatus(moduleName, status);
        };
    }

    private void notifyListeners(String moduleName, Boolean success) {
        for (InstallListener listener : sSessions.get(moduleName)) {
            notifyListener(listener, success);
        }

        sSessions.remove(moduleName);
        unregisterUpdateListener();
    }

    protected void notifyListener(InstallListener listener, Boolean success) {
        if (success) {
            mFacade.notifyObservers();
        }

        listener.onComplete(success);
    }

    private void registerUpdateListener() {
        if (sSessions.size() == 0) {
            mFacade.getSplitManager().registerListener(mUpdateListener);
        }
    }

    private void unregisterUpdateListener() {
        if (sSessions.size() == 0) {
            mFacade.getSplitManager().unregisterListener(mUpdateListener);
        }
    }

    @VisibleForTesting
    public void resetSessionQueue() {
        sSessions.clear();
    }
}
