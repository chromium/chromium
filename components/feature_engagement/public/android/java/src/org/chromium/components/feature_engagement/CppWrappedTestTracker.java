// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import android.text.TextUtils;

import androidx.annotation.CheckResult;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Callback;

/**
 * CppWrappedTestTracker is a Java implementation of a {@link Tracker} object that is encapsulated
 * by a Tracker object in C++, which will proxy (most) calls over from C++ to Java.
 *
 * <p>NOTE: For testing, most of the time this class is overkill and it suffices to create a test
 * object that derives from Tracker and call TrackerFactory#setTrackerForTests on it. However, this
 * will only replace the Tracker object on the Java side, and that object will never receive the
 * notifyEvent calls that the actual tracker in C++ receives. So, if receiving events is important
 * for your test, you may need {@link CppWrapperTestTracker}.
 *
 * <p>For example usage in tests, see below.
 */
@JNINamespace("feature_engagement")
public class CppWrappedTestTracker implements Tracker {
    // Example usage in tests:
    //
    //   mTracker = new CppWrappedTestTracker(FeatureConstants.YOU_FEATURE_HERE) {
    //       @Override
    //       public void notifyEvent(String event) {
    //           super.notifyEvent(event);
    //           // Validate that the right event was received.
    //       }
    //   };
    //   TrackerFactory.getTrackerForProfile(profile).injectTracker(mTracker);

    private String mOurFeature;
    private boolean mWasDismissed;
    private String mLastEvent;

    public CppWrappedTestTracker(String feature) {
        mOurFeature = feature;
    }

    public boolean wasDismissed() {
        return mWasDismissed;
    }

    public String getLastEvent() {
        return mLastEvent;
    }

    @CalledByNative
    @Override
    public void notifyEvent(String event) {
        mLastEvent = event;
    }

    @CheckResult
    @CalledByNative
    @Override
    public boolean shouldTriggerHelpUI(String feature) {
        return ourFeature(feature);
    }

    @CheckResult
    @Override
    public TriggerDetails shouldTriggerHelpUIWithSnooze(String feature) {
        return new TriggerDetails(ourFeature(feature), false);
    }

    @CalledByNative
    @Override
    public boolean wouldTriggerHelpUI(String feature) {
        return ourFeature(feature);
    }

    @CalledByNative
    @Override
    public boolean hasEverTriggered(String feature, boolean fromWindow) {
        return true;
    }

    @TriggerState
    @CalledByNative
    @Override
    public int getTriggerState(String feature) {
        return ourFeature(feature)
                ? TriggerState.HAS_NOT_BEEN_DISPLAYED
                : TriggerState.HAS_BEEN_DISPLAYED;
    }

    @CalledByNative
    @Override
    public void dismissed(String feature) {
        if (ourFeature(feature)) {
            mWasDismissed = true;
        }
    }

    @CalledByNative
    @Override
    public void dismissedWithSnooze(String feature, int snoozeAction) {
        if (ourFeature(feature)) {
            mWasDismissed = true;
        }
    }

    @CheckResult
    @Nullable
    @Override
    public DisplayLockHandle acquireDisplayLock() {
        assert false : "This should only be called on a production tracker";
        return () -> {};
    }

    @Override
    public void setPriorityNotification(String feature) {}

    @Override
    @Nullable
    public String getPendingPriorityNotification() {
        return null;
    }

    @Override
    public void registerPriorityNotificationHandler(
            String feature, Runnable priorityNotificationHandler) {}

    @Override
    public void unregisterPriorityNotificationHandler(String feature) {}

    @CalledByNative
    @Override
    public boolean isInitialized() {
        return true;
    }

    @Override
    public void addOnInitializedCallback(Callback<Boolean> callback) {
        assert false : "This should only be called on a production tracker";
        callback.onResult(true);
    }

    private boolean ourFeature(String feature) {
        return TextUtils.equals(mOurFeature, feature);
    }
}
