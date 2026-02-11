// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.security.advancedprotection.AdvancedProtectionManager;

import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.os.BuildCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Placeholder provider class to query whether the operating system has granted various security
 * permissions.
 */
@NullMarked
public class OsAdditionalSecurityProvider {
    public interface Observer {
        /** Called when the Android-OS advanced-protection-mode setting changes. */
        void onAdvancedProtectionOsSettingChanged();
    }

    private boolean mIsAdvancedProtectionRequestedByOs;

    private final ObserverList<Observer> mObservers = new ObserverList<Observer>();

    public OsAdditionalSecurityProvider() {
        if (!BuildCompat.isAtLeastB()) {
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        if (ContextCompat.checkSelfPermission(
                        context, Manifest.permission.QUERY_ADVANCED_PROTECTION_MODE)
                != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        var manager =
                (AdvancedProtectionManager)
                        context.getSystemService(Context.ADVANCED_PROTECTION_SERVICE);
        if (manager == null) {
            return;
        }

        mIsAdvancedProtectionRequestedByOs = manager.isAdvancedProtectionEnabled();
        manager.registerAdvancedProtectionCallback(
                Runnable::run,
                isAdvancedProtectionEnabled -> {
                    if (isAdvancedProtectionEnabled == mIsAdvancedProtectionRequestedByOs) {
                        return;
                    }

                    ThreadUtils.postOnUiThread(
                            () -> {
                                mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionEnabled;
                                for (Observer observer : mObservers) {
                                    observer.onAdvancedProtectionOsSettingChanged();
                                }
                            });
                });
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Returns whether the Android OS requests advanced-protection-mode. Implementations must allow
     * querying from any thread.
     */
    public boolean isAdvancedProtectionRequestedByOs() {
        return mIsAdvancedProtectionRequestedByOs;
    }

    /** Returns intent which launches OS-advanced-protection settings. */
    @Nullable
    public Intent getIntentForOsAdvancedProtectionSettings() {
        Intent intent = new Intent("com.google.android.gms.settings.ADVANCED_PROTECTION");
        intent.setPackage("com.google.android.gms");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }
}
