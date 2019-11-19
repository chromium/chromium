// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;
import android.util.SparseLongArray;

import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.RecordHistogram;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Record start time in order to later report the install duration via UMA. We want to make
 * a difference between modules that have been requested first before and after the last
 * Chrome start. Modules that have been requested before may install quicker as they may be
 * installed form cache. To do this, we use shared prefs to track modules previously
 * requested. Additionally, storing requested modules helps us to record module install
 * status at next Chrome start.
 */
public class SplitAvailabilityLogger {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    private static final int REQUESTED = 0;
    private static final int INSTALLED_REQUESTED = 1;
    private static final int INSTALLED_UNREQUESTED = 2;

    // Keep this one at the end and increment appropriately when adding new status.
    private static final int COUNT = 3;

    private static final String ONDEMAND_REQ_PREV = "key_modules_requested_previously";
    private static final String DEFERRED_REQ_PREV = "key_modules_deferred_requested_previously";

    private final Map<String, InstallTimes> mInstallTimesMap = new HashMap<>();

    /**
     * Records via UMA all modules that have been requested and are currently installed.
     */
    public static void logModuleAvailability() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> requestedModules = new HashSet<>();
        requestedModules.addAll(prefs.getStringSet(ONDEMAND_REQ_PREV, new HashSet<>()));
        requestedModules.addAll(prefs.getStringSet(DEFERRED_REQ_PREV, new HashSet<>()));

        Context context = ContextUtils.getApplicationContext();
        SplitInstallManager manager = SplitInstallManagerFactory.create(context);
        Set<String> installedModules = manager.getInstalledModules();

        for (String name : requestedModules) {
            recordAvailabilityStatus(
                    name, installedModules.contains(name) ? INSTALLED_REQUESTED : REQUESTED);
        }

        for (String name : installedModules) {
            if (!requestedModules.contains(name)) {
                recordAvailabilityStatus(name, INSTALLED_UNREQUESTED);
            }
        }
    }

    private static void recordAvailabilityStatus(String moduleName, int status) {
        String key = "Android.FeatureModules.AvailabilityStatus." + moduleName;
        EnumeratedHistogramSample sample = new EnumeratedHistogramSample(key, COUNT);
        sample.record(status);
    }

    /**
     * Records via UMA module install times divided into install steps.
     *
     * @param moduleName The module name.
     */
    public void logInstallTimes(String moduleName) {
        recordInstallTime(moduleName, "", SplitInstallSessionStatus.UNKNOWN,
                SplitInstallSessionStatus.INSTALLED);
        recordInstallTime(moduleName, ".PendingDownload", SplitInstallSessionStatus.UNKNOWN,
                SplitInstallSessionStatus.DOWNLOADING);
        recordInstallTime(moduleName, ".Download", SplitInstallSessionStatus.DOWNLOADING,
                SplitInstallSessionStatus.INSTALLING);
        recordInstallTime(moduleName, ".Installing", SplitInstallSessionStatus.INSTALLING,
                SplitInstallSessionStatus.INSTALLED);
    }

    /**
     * Records the start time of an on-demand install request.
     *
     * @param moduleName The module name.
     */
    public void storeRequestStart(String moduleName) {
        assert !mInstallTimesMap.containsKey(moduleName);
        boolean moduleRequested = storeModuleRequested(moduleName, ONDEMAND_REQ_PREV);
        mInstallTimesMap.put(moduleName, new InstallTimes(moduleRequested));
    }

    /**
     * Records module deferred requested.
     *
     * @param moduleName The module name.
     */
    public void storeRequestDeferredStart(String moduleName) {
        storeModuleRequested(moduleName, DEFERRED_REQ_PREV);
    }

    /**
     * Records that a module has been installed on-demand.
     *
     * @param moduleName The module name.
     * @param status The install status.
     */
    public void storeModuleInstalled(String moduleName, int status) {
        InstallTimes times = mInstallTimesMap.get(moduleName);
        times.mInstallTimes.put(status, SystemClock.uptimeMillis());
    }

    /**
     * Stores to shared prevs that a module has been requested.
     *
     * @param moduleName Module that has been requested.
     * @param prefKey Pref key pointing to a string set to which the requested module will be added.
     * @return Whether the module has been requested previously.
     */
    private boolean storeModuleRequested(String moduleName, String prefKey) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> modulesRequestedPreviously = prefs.getStringSet(prefKey, new HashSet<>());
        Set<String> newModulesRequestedPreviously = new HashSet<>(modulesRequestedPreviously);
        newModulesRequestedPreviously.add(moduleName);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(prefKey, newModulesRequestedPreviously);
        editor.apply();
        return modulesRequestedPreviously.contains(moduleName);
    }

    private void recordInstallTime(
            String moduleName, String histogramSubname, int startKey, int endKey) {
        assert mInstallTimesMap.containsKey(moduleName);

        InstallTimes installTimes = mInstallTimesMap.get(moduleName);
        long startTime = installTimes.mInstallTimes.get(startKey);
        long endTime = installTimes.mInstallTimes.get(endKey);

        if (startTime == 0 || endTime == 0) {
            // Time stamps for install times have not been stored.
            // Don't record anything to not skew data.
            return;
        }

        String cacheKey = installTimes.mIsCached ? "Cached" : "Uncached";
        long timing = endTime - startTime;
        String key = String.format("Android.FeatureModules.%sAwakeInstallDuration%s.%s", cacheKey,
                histogramSubname, moduleName);

        RecordHistogram.recordLongTimesHistogram(key, timing);
    }

    private static class InstallTimes {
        public final boolean mIsCached;
        public final SparseLongArray mInstallTimes = new SparseLongArray();

        public InstallTimes(boolean isCached) {
            mIsCached = isCached;
            mInstallTimes.put(SplitInstallSessionStatus.UNKNOWN, SystemClock.uptimeMillis());
        }
    }
}
