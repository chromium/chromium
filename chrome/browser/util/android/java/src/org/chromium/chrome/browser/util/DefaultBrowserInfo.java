// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/** A utility class for querying information about the default browser setting. */
@NullMarked
public final class DefaultBrowserInfo {
    static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/40697015): move to some util class for reuse.
    static final String[] CHROME_PRE_STABLE_PACKAGE_NAMES = {
        "org.chromium.chrome", "com.chrome.canary", "com.chrome.beta", "com.chrome.dev"
    };

    //  LINT.IfChange(AndroidDefaultBrowserState)
    @IntDef({
        DefaultBrowserState.NO_DEFAULT,
        DefaultBrowserState.OTHER_DEFAULT,
        DefaultBrowserState.CHROME_DEFAULT,
        DefaultBrowserState.OTHER_CHROME_DEFAULT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserState {
        int NO_DEFAULT = 0;
        int OTHER_DEFAULT = 1;

        /**
         * CHROME_DEFAULT means the currently running Chrome channel is default. As opposed to
         * OTHER_CHROME_DEFAULT which looks for all Chrome channels.
         */
        int CHROME_DEFAULT = 2;

        /** Whether other Chrome package (except the current running one) is default. */
        int OTHER_CHROME_DEFAULT = 3;

        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidDefaultBrowserState)

    /** Contains all status related to the default browser state on the device. */
    public static class DefaultInfo {
        /** The current default browser state on the device. */
        public final @DefaultBrowserState int defaultBrowserState;

        /** Whether or not Chrome is the system browser. */
        public final boolean isChromeSystem;

        /** Whether or not the default browser is the system browser. */
        public final boolean isDefaultSystem;

        /** The number of browsers installed on this device. */
        public final int browserCount;

        /** The number of system browsers installed on this device. */
        public final int systemCount;

        public final boolean isChromePreStableInstalled;

        /** Creates an instance of the {@link DefaultInfo} class. */
        public DefaultInfo(
                @DefaultBrowserState int defaultBrowserState,
                boolean isChromeSystem,
                boolean isDefaultSystem,
                int browserCount,
                int systemCount,
                boolean isChromePreStableInstalled) {
            this.defaultBrowserState = defaultBrowserState;
            this.isChromeSystem = isChromeSystem;
            this.isDefaultSystem = isDefaultSystem;
            this.browserCount = browserCount;
            this.systemCount = systemCount;
            this.isChromePreStableInstalled = isChromePreStableInstalled;
        }
    }

    private static @Nullable DefaultInfoTask sDefaultInfoTask;

    /** Don't instantiate me. */
    private DefaultBrowserInfo() {}

    /**
     * Determines various information about browsers on the system.
     *
     * @param callback To be called with a {@link DefaultInfo} instance if possible. Can be {@code
     *     null}.
     * @see DefaultInfo
     */
    public static void getDefaultBrowserInfo(Callback<@Nullable DefaultInfo> callback) {
        ThreadUtils.checkUiThread();
        if (sDefaultInfoTask == null) sDefaultInfoTask = new DefaultInfoTask();
        sDefaultInfoTask.get(callback);
    }

    /** Cancel and reset the current DefaultInfoTask. */
    public static void resetDefaultInfoTask() {
        if (sDefaultInfoTask != null) {
            sDefaultInfoTask.cancel(false);
            sDefaultInfoTask = null;
        }
    }

    public static void setDefaultInfoForTests(DefaultInfo info) {
        DefaultInfoTask.setDefaultInfoForTests(info);
    }

    public static void clearDefaultInfoForTests() {
        DefaultInfoTask.clearDefaultInfoForTests();
    }

    private static class DefaultInfoTask extends AsyncTask<DefaultInfo> {
        private static @Nullable AtomicReference<DefaultInfo> sTestInfo;

        private final ObserverList<Callback<@Nullable DefaultInfo>> mObservers =
                new ObserverList<>();

        public static void setDefaultInfoForTests(DefaultInfo info) {
            sTestInfo = new AtomicReference<>(info);
        }

        public static void clearDefaultInfoForTests() {
            sTestInfo = null;
        }

        /**
         * Queues up {@code callback} to be notified of the result of this {@link AsyncTask}. If the
         * task has not been started, this will start it. If the task is finished, this will send
         * the result. If the task is running this will queue the callback up until the task is
         * done.
         *
         * @param callback The {@link Callback} to notify with the right {@link DefaultInfo}.
         */
        public void get(Callback<@Nullable DefaultInfo> callback) {
            ThreadUtils.checkUiThread();

            if (getStatus() == Status.FINISHED) {
                DefaultInfo info = null;
                try {
                    info = sTestInfo == null ? get() : sTestInfo.get();
                } catch (InterruptedException | ExecutionException e) {
                    // Fail silently here since this is not a critical task.
                }

                final DefaultInfo postInfo = info;
                PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(postInfo));
            } else {
                if (getStatus() == Status.PENDING) {
                    try {
                        executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    } catch (RejectedExecutionException e) {
                        // Fail silently here since this is not a critical task.
                        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
                        return;
                    }
                }
                mObservers.addObserver(callback);
            }
        }

        @Override
        protected DefaultInfo doInBackground() {
            Context context = ContextUtils.getApplicationContext();

            @DefaultBrowserState int defaultBrowserState = DefaultBrowserState.NO_DEFAULT;
            boolean isChromeSystem = false;
            boolean isDefaultSystem = false;
            boolean isChromePreStableInstalled = false;
            int systemCount = 0;

            // Query the default handler first.
            ResolveInfo defaultRi = PackageManagerUtils.resolveDefaultWebBrowserActivity();
            if (defaultRi != null && defaultRi.match != 0) {
                if (isSamePackage(context, defaultRi)) {
                    defaultBrowserState = DefaultBrowserState.CHROME_DEFAULT;
                } else if (CHROME_STABLE_PACKAGE_NAME.equals(
                                defaultRi.activityInfo.applicationInfo.packageName)
                        || isChromePreStable(defaultRi)) {
                    defaultBrowserState = DefaultBrowserState.OTHER_CHROME_DEFAULT;
                } else {
                    defaultBrowserState = DefaultBrowserState.OTHER_DEFAULT;
                }
                isDefaultSystem = isSystemPackage(defaultRi);
            }

            // Query all other intent handlers.
            Set<String> uniquePackages = new HashSet<>();
            List<ResolveInfo> ris = PackageManagerUtils.queryAllWebBrowsersInfo();
            if (ris != null) {
                for (ResolveInfo ri : ris) {
                    String packageName = ri.activityInfo.applicationInfo.packageName;
                    if (!uniquePackages.add(packageName)) continue;

                    if (isSystemPackage(ri)) {
                        if (isSamePackage(context, ri)) isChromeSystem = true;
                        systemCount++;
                    }

                    if (isChromePreStable(ri)) {
                        isChromePreStableInstalled = true;
                    }
                }
            }

            int browserCount = uniquePackages.size();

            return new DefaultInfo(
                    defaultBrowserState,
                    isChromeSystem,
                    isDefaultSystem,
                    browserCount,
                    systemCount,
                    isChromePreStableInstalled);
        }

        @Override
        protected void onPostExecute(DefaultInfo defaultInfo) {
            flushCallbacks(sTestInfo == null ? defaultInfo : sTestInfo.get());
        }

        @Override
        protected void onCancelled() {
            flushCallbacks(null);
        }

        private void flushCallbacks(@Nullable DefaultInfo info) {
            for (Callback<@Nullable DefaultInfo> callback : mObservers) {
                callback.onResult(info);
            }
            mObservers.clear();
        }
    }

    private static boolean isSamePackage(Context context, ResolveInfo info) {
        return TextUtils.equals(
                context.getPackageName(), info.activityInfo.applicationInfo.packageName);
    }

    private static boolean isSystemPackage(ResolveInfo info) {
        return (info.activityInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }

    private static boolean isChromePreStable(ResolveInfo info) {
        for (String name : CHROME_PRE_STABLE_PACKAGE_NAMES) {
            if (name.equals(info.activityInfo.applicationInfo.packageName)) return true;
        }
        return false;
    }
}
