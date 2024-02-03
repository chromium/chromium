// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.ComponentName;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.SparseArray;
import android.view.View;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;
import android.view.autofill.VirtualViewFillInfo;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.build.annotations.DoNotStripLogs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** The class to call Android's AutofillManager. */
public class AutofillManagerWrapper {
    // Don't change TAG, it is used for runtime log.
    // NOTE: As a result of the above, the tag below still references the name of this class from
    // when it was originally developed specifically for Android WebView.
    public static final String TAG = "AwAutofillManager";
    private static final String AWG_COMPONENT_NAME =
            "com.google.android.gms/com.google.android.gms.autofill.service.AutofillService";

    /** The observer of suggestion window. */
    public static interface InputUIObserver {
        void onInputUIShown();
    }

    private static class AutofillInputUIMonitor extends AutofillManager.AutofillCallback {
        private WeakReference<AutofillManagerWrapper> mManager;

        public AutofillInputUIMonitor(AutofillManagerWrapper manager) {
            mManager = new WeakReference<AutofillManagerWrapper>(manager);
        }

        @Override
        public void onAutofillEvent(View view, int virtualId, int event) {
            AutofillManagerWrapper manager = mManager.get();
            if (manager == null) return;
            manager.mIsAutofillInputUIShowing = (event == EVENT_INPUT_SHOWN);
            if (event == EVENT_INPUT_SHOWN) manager.notifyInputUIChange();
        }
    }

    private static boolean sIsLoggable;
    private final String mPackageName;
    private AutofillManager mAutofillManager;
    private boolean mIsAutofillInputUIShowing;
    private AutofillInputUIMonitor mMonitor;
    private boolean mDestroyed;
    private boolean mDisabled;
    private ArrayList<WeakReference<InputUIObserver>> mInputUIObservers;
    // Indicates if AwG is the current Android autofill service.
    private final boolean mIsAwGCurrentAutofillService;

    public AutofillManagerWrapper(Context context) {
        updateLogStat();
        if (isLoggable()) log("constructor");
        mAutofillManager = context.getSystemService(AutofillManager.class);
        mDisabled = mAutofillManager == null || !mAutofillManager.isEnabled();

        if (mDisabled) {
            mPackageName = "";
            mIsAwGCurrentAutofillService = false;
            if (isLoggable()) log("disabled");
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            ComponentName componentName = null;
            try {
                componentName = mAutofillManager.getAutofillServiceComponentName();
            } catch (Exception e) {
                // Can't catch com.android.internal.util.SyncResultReceiver.TimeoutException,
                // because
                // - The exception isn't Android API.
                // - Different version of Android handle it differently.
                // Uses Exception to catch various cases. (refer to crbug.com/1186406)
                Log.e(TAG, "getAutofillServiceComponentName", e);
            }
            if (componentName != null) {
                mPackageName = componentName.getPackageName();
                mIsAwGCurrentAutofillService =
                        AWG_COMPONENT_NAME.equals(componentName.flattenToString());
                AutofillProviderUMA.logCurrentProvider(mPackageName);
            } else {
                mPackageName = "";
                mIsAwGCurrentAutofillService = false;
            }
        } else {
            mPackageName = "";
            mIsAwGCurrentAutofillService = false;
        }
        mMonitor = new AutofillInputUIMonitor(this);
        mAutofillManager.registerCallback(mMonitor);
    }

    public String getPackageName() {
        return mPackageName;
    }

    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualValueChanged");
        mAutofillManager.notifyValueChanged(parent, childId, value);
    }

    public void commit(int submissionSource) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("commit source:" + submissionSource);
        mAutofillManager.commit();
    }

    public void cancel() {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("cancel");
        mAutofillManager.cancel();
    }

    public void notifyVirtualViewsReady(
            View parent, SparseArray<VirtualViewFillInfo> viewFillInfos) {
        // notifyVirtualViewsReady was added in Android U.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        if (mDisabled || checkAndWarnIfDestroyed()) return;

        if (isLoggable()) log("notifyVirtualViewsReady");
        mAutofillManager.notifyVirtualViewsReady(parent, viewFillInfos);
    }

    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        // Log warning only when the autofill is triggered.
        if (mDisabled) {
            Log.w(TAG, "Autofill is disabled: AutofillManager isn't available in given Context.");
            return;
        }
        if (checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewEntered");
        mAutofillManager.notifyViewEntered(parent, childId, absBounds);
    }

    @RequiresApi(VERSION_CODES.TIRAMISU)
    public boolean showAutofillDialog(View parent, int childId) {
        // Log warning only when the autofill is triggered.
        if (mDisabled) {
            Log.w(TAG, "Autofill is disabled: AutofillManager isn't available in given Context.");
            return false;
        }
        if (checkAndWarnIfDestroyed()) return false;
        if (isLoggable()) log("showAutofillDialog");
        return mAutofillManager.showAutofillDialog(parent, childId);
    }

    public void notifyVirtualViewExited(View parent, int childId) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewExited");
        mAutofillManager.notifyViewExited(parent, childId);
    }

    public void notifyVirtualViewVisibilityChanged(View parent, int childId, boolean isVisible) {
        // `notifyViewVisibilityChanged` was added in API level 27.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O_MR1) return;
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewVisibilityChanged");
        mAutofillManager.notifyViewVisibilityChanged(parent, childId, isVisible);
    }

    public void requestAutofill(View parent, int virtualId, Rect absBounds) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("requestAutofill");
        mAutofillManager.requestAutofill(parent, virtualId, absBounds);
    }

    public boolean isAutofillInputUIShowing() {
        if (mDisabled || checkAndWarnIfDestroyed()) return false;
        if (isLoggable()) log("isAutofillInputUIShowing: " + mIsAutofillInputUIShowing);
        return mIsAutofillInputUIShowing;
    }

    public void destroy() {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("destroy");
        try {
            // The binder in the autofill service side might already be dropped,
            // unregisterCallback() will cause various exceptions in this
            // scenario (see crbug.com/1078337), catching RuntimeException here prevents crash.
            mAutofillManager.unregisterCallback(mMonitor);
        } catch (RuntimeException e) {
            // We are not logging anything here since some of the exceptions are raised as 'generic'
            // RuntimeException which makes it difficult to catch and ignore separately; and the
            // RuntimeException seemed only happen in Android O, therefore, isn't actionable.
        } finally {
            mAutofillManager = null;
            mDestroyed = true;
        }
    }

    public boolean isDisabled() {
        return mDisabled;
    }

    /**
     * Only work for Android P and beyond. Always return false for Android O.
     * @return if the Autofill with Google is the current autofill service.
     */
    public boolean isAwGCurrentAutofillService() {
        return mIsAwGCurrentAutofillService;
    }

    private boolean checkAndWarnIfDestroyed() {
        if (mDestroyed) {
            Log.w(
                    TAG,
                    "Application attempted to call on a destroyed AutofillManagerWrapper",
                    new Throwable());
        }
        return mDestroyed;
    }

    public void addInputUIObserver(InputUIObserver observer) {
        if (observer == null) return;
        if (mInputUIObservers == null) {
            mInputUIObservers = new ArrayList<WeakReference<InputUIObserver>>();
        }
        mInputUIObservers.add(new WeakReference<InputUIObserver>(observer));
    }

    @VisibleForTesting
    public void notifyInputUIChange() {
        for (InputUIObserver observer : CollectionUtil.strengthen(mInputUIObservers)) {
            observer.onInputUIShown();
        }
    }

    public void notifyNewSessionStarted(boolean hasServerPrediction) {
        updateLogStat();
        if (isLoggable()) log("Session starts, has server prediction = " + hasServerPrediction);
    }

    public void onServerPredictionsAvailable() {
        if (isLoggable()) log("Server predictions available");
    }

    /** Always check isLoggable() before call this method. */
    public static void log(String log) {
        // Log.i() instead of Log.d() is used here because log.d() is stripped out in release build.
        Log.i(TAG, log);
    }

    public static boolean isLoggable() {
        return sIsLoggable;
    }

    @DoNotStripLogs
    private static void updateLogStat() {
        // Use 'setprop log.tag.AwAutofillManager DEBUG' to enable the log at runtime.
        // NOTE: See the comment on TAG above for why this is still AwAutofillManager.
        // Check the system setting directly.
        sIsLoggable = android.util.Log.isLoggable(TAG, Log.DEBUG);
    }

}
