// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import android.app.ApplicationErrorReport;
import android.os.Build;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;
import android.os.strictmode.DiskReadViolation;
import android.os.strictmode.DiskWriteViolation;
import android.os.strictmode.ResourceMismatchViolation;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;

/**
 * StrictMode whitelist installer.
 *
 * <p><b>How this works:</b><br>
 * When StrictMode is enabled without the death penalty, it queues up all ThreadPolicy violations
 * into a ThreadLocal ArrayList, and then posts a Runnable to the start of the Looper queue to
 * process them. This is done in order to set a cap to the number of logged/handled violations per
 * event loop, and avoid overflowing the log buffer or other penalty handlers with violations. <br>
 * Because the violations are queued into a ThreadLocal ArrayList, they must be queued on the
 * offending thread, and thus the offending stack frame will exist in the stack trace. The
 * whitelisting mechanism works by using reflection to set a custom ArrayList into the ThreadLocal.
 * When StrictMode is adding a new item to the ArrayList, our custom ArrayList checks the stack
 * trace for any whitelisted frames, and if one is found, no-ops the addition. Then, when the
 * processing runnable executes, it sees there are no items, and no-ops. <br>
 * However, if the death penalty is enabled, the concern about handling too many violations no
 * longer exists (since death will occur after the first one), so the queue is bypassed, and death
 * occurs instantly without allowing the whitelisting system to intercept it. In order to retain the
 * death penalty, the whitelisting mechanism itself can be configured to execute the death penalty
 * after the first non-whitelisted violation.
 */
final class ReflectiveThreadStrictModeInterceptor implements ThreadStrictModeInterceptor {
    private static final String TAG = "ThreadStrictMode";

    @NonNull private final List<Function<Violation, Integer>> mWhitelistEntries;
    @Nullable private final Consumer mCustomPenalty;

    ReflectiveThreadStrictModeInterceptor(
            @NonNull List<Function<Violation, Integer>> whitelistEntries,
            @Nullable Consumer customPenalty) {
        mWhitelistEntries = whitelistEntries;
        mCustomPenalty = customPenalty;
    }

    @Override
    public void install(ThreadPolicy detectors) {
        // Use reflection on Android P despite the existence of
        // StrictMode.OnThreadViolationListener because the listener receives a stack
        // trace with stack frames prior to android.os.Handler calls stripped out.

        interceptWithReflection();
        StrictMode.setThreadPolicy(new ThreadPolicy.Builder(detectors).penaltyLog().build());
    }

    private void interceptWithReflection() {
        ThreadLocal<ArrayList<Object>> violationsBeingTimed;
        try {
            violationsBeingTimed = getViolationsBeingTimed();
        } catch (Exception e) {
            throw new RuntimeException(null, e);
        }
        violationsBeingTimed.get().clear();
        violationsBeingTimed.set(
                new ArrayList<Object>() {
                    @Override
                    public boolean add(Object o) {
                        int violationType = getViolationType(o);
                        StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();
                        Violation violation =
                                new Violation(
                                        violationType,
                                        Arrays.copyOf(stackTrace, stackTrace.length));
                        if (violationType != Violation.DETECT_FAILED
                                && violation.isInWhitelist(mWhitelistEntries)) {
                            return true;
                        }
                        if (mCustomPenalty != null) {
                            mCustomPenalty.accept(violation);
                        }
                        return super.add(o);
                    }
                });
    }

    @SuppressWarnings({"unchecked"})
    private static ThreadLocal<ArrayList<Object>> getViolationsBeingTimed()
            throws IllegalAccessException, NoSuchFieldException {
        Field violationTimingField = StrictMode.class.getDeclaredField("violationsBeingTimed");
        violationTimingField.setAccessible(true);
        return (ThreadLocal<ArrayList<Object>>) violationTimingField.get(null);
    }

    /** @param o {@code android.os.StrictMode.ViolationInfo} */
    @SuppressWarnings({"unchecked", "DiscouragedPrivateApi", "PrivateApi", "BlockedPrivateApi"})
    private int getViolationType(Object violationInfo) {
        try {
            Class<?> violationInfoClass = Class.forName("android.os.StrictMode$ViolationInfo");
            if (Build.VERSION.SDK_INT == 28) {
                Method getViolationBitMethod =
                        violationInfoClass.getDeclaredMethod("getViolationBit");
                getViolationBitMethod.setAccessible(true);
                int violationType = (Integer) getViolationBitMethod.invoke(violationInfo);
                return violationType & Violation.DETECT_ALL_KNOWN;
            } else if (Build.VERSION.SDK_INT == 29) {
                Method getViolationClassMethod =
                        violationInfoClass.getDeclaredMethod("getViolationClass");
                getViolationClassMethod.setAccessible(true);
                return computeViolationTypeAndroid10(
                        (Class<?>) getViolationClassMethod.invoke(violationInfo));
            } else if (Build.VERSION.SDK_INT >= 30) {
                // ViolationInfo#getViolationClass() is inaccessible via reflection.
                // crbug.com/1240777 Ignore violation type when checking white list.
                return Violation.DETECT_ALL_KNOWN;
            }
            Field crashInfoField = violationInfoClass.getDeclaredField("crashInfo");
            crashInfoField.setAccessible(true);
            ApplicationErrorReport.CrashInfo crashInfo =
                    (ApplicationErrorReport.CrashInfo) crashInfoField.get(violationInfo);
            Method parseViolationFromMessage =
                    StrictMode.class.getDeclaredMethod("parseViolationFromMessage", String.class);
            parseViolationFromMessage.setAccessible(true);
            int mask =
                    (int)
                            parseViolationFromMessage.invoke(
                                    /* static= */ null, crashInfo.exceptionMessage);
            return mask & Violation.DETECT_ALL_KNOWN;
        } catch (Exception e) {
            Log.e(TAG, "Unable to get violation.", e);
            return Violation.DETECT_FAILED;
        }
    }

    /** Computes the violation type based on the class of the passed-in violation. */
    @RequiresApi(29)
    private static int computeViolationTypeAndroid10(Class<?> violationClass) {
        if (DiskReadViolation.class.isAssignableFrom(violationClass)) {
            return Violation.DETECT_DISK_READ;
        } else if (DiskWriteViolation.class.isAssignableFrom(violationClass)) {
            return Violation.DETECT_DISK_WRITE;
        } else if (ResourceMismatchViolation.class.isAssignableFrom(violationClass)) {
            return Violation.DETECT_RESOURCE_MISMATCH;
        }
        return Violation.DETECT_FAILED;
    }
}
