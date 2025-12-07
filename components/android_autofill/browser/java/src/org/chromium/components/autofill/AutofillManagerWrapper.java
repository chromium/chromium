// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.build.annotations.DoNotStripLogs;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** The class to call Android's AutofillManager. */
@NullMarked
public class AutofillManagerWrapper {
    // Don't change TAG, it is used for runtime log.
    // NOTE: As a result of the above, the tag below still references the name of this class from
    // when it was originally developed specifically for Android WebView.
    public static final String TAG = "AwAutofillManager";
    private static final String AWG_COMPONENT_NAME =
            "com.google.android.gms/com.google.android.gms.autofill.service.AutofillService";

    /** The observer of suggestion window. */
    public interface InputUiObserver {
        void onInputUiShown();
    }

    private static class AutofillInputUiMonitor extends AutofillManager.AutofillCallback {
        private final WeakReference<AutofillManagerWrapper> mManager;

        public AutofillInputUiMonitor(AutofillManagerWrapper manager) {
            mManager = new WeakReference<>(manager);
        }

        @Override
        public void onAutofillEvent(View view, int virtualId, int event) {
            AutofillManagerWrapper manager = mManager.get();
            if (manager == null) return;
            manager.mIsAutofillInputUiShowing = (event == EVENT_INPUT_SHOWN);
            if (event == EVENT_INPUT_SHOWN) manager.notifyInputUiChange();
        }
    }

    private static boolean sIsLoggable;
    private final String mPackageName;
    private @Nullable AutofillManager mAutofillManager;
    private boolean mIsAutofillInputUiShowing;
    private @Nullable AutofillInputUiMonitor mMonitor;
    private boolean mDestroyed;
    private @Nullable ArrayList<WeakReference<InputUiObserver>> mInputUiObservers;
    // Indicates if AwG is the current Android autofill service.
    private final boolean mIsAwGCurrentAutofillService;

    public static boolean isEnabled(AutofillManager autofillManager) {
        try {
            return autofillManager != null && autofillManager.isEnabled();
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.IS_ENABLED);
            return false;
        }
    }

    public static boolean isAutofillSupported(AutofillManager autofillManager) {
        try {
            return autofillManager != null && autofillManager.isAutofillSupported();
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.IS_AUTOFILL_SUPPORTED);
            return false;
        }
    }

    @RequiresApi(VERSION_CODES.P)
    public static @Nullable ComponentName getAutofillServiceComponentName(
            AutofillManager autofillManager) {
        try {
            return autofillManager.getAutofillServiceComponentName();
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e,
                    AutofillProviderUMA.AutofillManagerMethod.GET_AUTOFILL_SERVICE_COMPONENT_NAME);
            return null;
        }
    }

    public AutofillManagerWrapper(Context context) {
        updateLogStat();
        if (isLoggable()) log("constructor");

        mAutofillManager = retrieveAutofillManager(context);
        if (mAutofillManager == null) {
            mPackageName = "";
            mIsAwGCurrentAutofillService = false;
            if (isLoggable()) log("disabled");
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            ComponentName componentName = getAutofillServiceComponentName(mAutofillManager);
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
        mMonitor = new AutofillInputUiMonitor(this);
        try {
            mAutofillManager.registerCallback(mMonitor);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.REGISTER_CALLBACK);
        }
    }

    private static @Nullable AutofillManager retrieveAutofillManager(@Nullable Context context) {
        if (context == null) return null;
        if (context == ContextUtils.getApplicationContext()) {
            if (isLoggable()) log("Created with application context.");
        }
        return context.getSystemService(AutofillManager.class);
    }

    public String getPackageName() {
        return mPackageName;
    }

    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualValueChanged");
        try {
            mAutofillManager.notifyValueChanged(parent, childId, value);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.NOTIFY_VALUE_CHANGED);
        }
    }

    public void commit(int submissionSource) {
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("commit source:" + submissionSource);
        try {
            mAutofillManager.commit();
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.COMMIT);
        }
    }

    public void cancel() {
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("cancel");
        try {
            mAutofillManager.cancel();
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.CANCEL);
        }
    }

    public void notifyVirtualViewsReady(
            View parent, SparseArray<VirtualViewFillInfo> viewFillInfos) {
        // notifyVirtualViewsReady was added in Android U.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewsReady");
        try {
            mAutofillManager.notifyVirtualViewsReady(parent, viewFillInfos);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.NOTIFY_VIRTUAL_VIEWS_READY);
        }
    }

    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        // Log warning only when the autofill is triggered.
        if (isDisabled()) {
            Log.w(TAG, "Autofill is disabled: AutofillManager isn't available in given Context.");
            return;
        }
        if (checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewEntered");
        try {
            mAutofillManager.notifyViewEntered(parent, childId, absBounds);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.NOTIFY_VIEW_ENTERED);
        }
    }

    @RequiresApi(VERSION_CODES.TIRAMISU)
    public boolean showAutofillDialog(View parent, int childId) {
        // Log warning only when the autofill is triggered.
        if (isDisabled()) {
            Log.w(TAG, "Autofill is disabled: AutofillManager isn't available in given Context.");
            return false;
        }
        if (checkAndWarnIfDestroyed()) return false;
        if (isLoggable()) log("showAutofillDialog");
        try {
            return mAutofillManager.showAutofillDialog(parent, childId);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.SHOW_AUTOFILL_DIALOG);
            return false;
        }
    }

    public void notifyVirtualViewExited(View parent, int childId) {
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewExited");
        try {
            mAutofillManager.notifyViewExited(parent, childId);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.NOTIFY_VIEW_EXITED);
        }
    }

    public void notifyVirtualViewVisibilityChanged(View parent, int childId, boolean isVisible) {
        // `notifyViewVisibilityChanged` was added in API level 27.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O_MR1) return;
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewVisibilityChanged");
        try {
            mAutofillManager.notifyViewVisibilityChanged(parent, childId, isVisible);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.NOTIFY_VIEW_VISIBILITY_CHANGED);
        }
    }

    public void requestAutofill(View parent, int virtualId, Rect absBounds) {
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("requestAutofill");
        try {
            mAutofillManager.requestAutofill(parent, virtualId, absBounds);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.REQUEST_AUTOFILL);
        }
    }

    public boolean isAutofillInputUiShowing() {
        if (isDisabled() || checkAndWarnIfDestroyed()) return false;
        if (isLoggable()) log("isAutofillInputUiShowing: " + mIsAutofillInputUiShowing);
        return mIsAutofillInputUiShowing;
    }

    public void destroy() {
        if (isDisabled() || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("destroy");
        try {
            // The binder in the autofill service side might already be dropped,
            // unregisterCallback() will cause various exceptions in this
            // scenario (see crbug.com/1078337), catching RuntimeException here prevents crash.
            mAutofillManager.unregisterCallback(mMonitor);
        } catch (Exception e) {
            AutofillProviderUMA.recordException(
                    e, AutofillProviderUMA.AutofillManagerMethod.UNREGISTER_CALLBACK);
        } finally {
            mAutofillManager = null;
            mDestroyed = true;
        }
    }

    @EnsuresNonNullIf(value = "mAutofillManager", result = false)
    public boolean isDisabled() {
        if (mAutofillManager == null || mDestroyed) {
            return true;
        }
        return !isEnabled(mAutofillManager);
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

    public void addInputUiObserver(InputUiObserver observer) {
        if (observer == null) return;
        if (mInputUiObservers == null) {
            mInputUiObservers = new ArrayList<>();
        }
        mInputUiObservers.add(new WeakReference<>(observer));
    }

    public void removeInputUiObserver(InputUiObserver observer) {
        if (observer == null || mInputUiObservers == null) return;
        mInputUiObservers.removeIf(observerRef -> observerRef.get() == observer);
    }

    @VisibleForTesting
    public void notifyInputUiChange() {
        assumeNonNull(mInputUiObservers);
        for (InputUiObserver observer : CollectionUtil.strengthen(mInputUiObservers)) {
            observer.onInputUiShown();
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
        // Log.i() instead of Log.d() is used here because Log.d() is stripped out in release build.
        Log.i(TAG, log);
    }

    public static boolean isLoggable() {
        return sIsLoggable;
    }

    @DoNotStripLogs
    @DoNotInline
    private static void updateLogStat() {
        // Use 'setprop log.tag.AwAutofillManager DEBUG' to enable the log at runtime.
        // NOTE: See the comment on TAG above for why this is still AwAutofillManager.
        // Check the system setting directly.
        sIsLoggable = android.util.Log.isLoggable(TAG, Log.DEBUG);
    }
}
