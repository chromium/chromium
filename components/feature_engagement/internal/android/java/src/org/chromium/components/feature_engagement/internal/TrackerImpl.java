// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement.internal;

import androidx.annotation.CheckResult;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.FeatureProperties;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerHelpUIResult;
import org.chromium.components.feature_engagement.UiFormat;
import org.chromium.ui.UiSwitches;

import java.util.function.BooleanSupplier;

/**
 * Java side of the JNI bridge between TrackerImpl in Java and C++. All method calls are delegated
 * to the native C++ class.
 */
@JNINamespace("feature_engagement")
@NullMarked
public class TrackerImpl implements Tracker {
    /**
     * A JNI-wrapper for the native DisplayLockHandle.
     * The C++ counterpart is DisplayLockHandleAndroid.
     */
    static class DisplayLockHandleAndroid implements DisplayLockHandle {
        @CalledByNative
        private static DisplayLockHandleAndroid create(long nativePtr) {
            return new DisplayLockHandleAndroid(nativePtr);
        }

        long mNativePtr;

        private DisplayLockHandleAndroid(long nativePtr) {
            mNativePtr = nativePtr;
        }

        @CalledByNative
        private void clearNativePtr() {
            mNativePtr = 0;
        }

        @Override
        public void release() {
            assert mNativePtr != 0;
            TrackerImplJni.get().release(mNativePtr);
            assert mNativePtr == 0;
        }
    }

    /** The pointer to the feature_engagement::TrackerImplAndroid JNI bridge. */
    private long mNativePtr;

    // Defaults to true; flag-guarded by kAndroidUserEducationFramework in TrackerFactory.
    private BooleanSupplier mPromotionsEnabledSupplier = SupplierUtils.alwaysTrueBooleanSupplier();

    // Defaults to false; flag-guarded by kAndroidUserEducationFramework and
    // kAggressivelyCooldownIphs in TrackerFactory.
    private BooleanSupplier mAggressivelyCooldownIphsSupplier =
            SupplierUtils.alwaysFalseBooleanSupplier();

    @CalledByNative
    @VisibleForTesting
    static TrackerImpl create(long nativePtr) {
        return new TrackerImpl(nativePtr);
    }

    private TrackerImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void setPromotionsEnabledSupplier(BooleanSupplier supplier) {
        mPromotionsEnabledSupplier = supplier;
    }

    @Override
    public void setAggressivelyCooldownIphsSupplier(BooleanSupplier supplier) {
        mAggressivelyCooldownIphsSupplier = supplier;
    }

    @Override
    public void notifyEvent(String event) {
        assert mNativePtr != 0;
        TrackerImplJni.get().notifyEvent(mNativePtr, event);
    }

    @Override
    @Deprecated
    public boolean shouldTriggerHelpUi(String feature) {
        FeatureProperties properties =
                new FeatureProperties(
                        UiFormat.UNKNOWN,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ false,
                        /* blocksCooldownGroup= */ false);

        // Call the new method to share enterprise policy check logic.
        if (!shouldTriggerHelpUi(feature, properties)) {
            return false;
        }
        if (mAggressivelyCooldownIphsSupplier.getAsBoolean()) {
            recordCooldownTriggered(UiFormat.DIALOG);
            recordCooldownTriggered(UiFormat.BLUE_BUBBLE);
        }
        return true;
    }

    @Override
    public boolean shouldTriggerHelpUi(String feature, FeatureProperties featureProperties) {
        return shouldTriggerHelpUiInternal(feature, featureProperties, /* withSnooze= */ false)
                .shouldTriggerIph;
    }

    @Override
    @Deprecated
    public TriggerDetails shouldTriggerHelpUiWithSnooze(String feature) {
        FeatureProperties properties =
                new FeatureProperties(
                        UiFormat.UNKNOWN,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ false,
                        /* blocksCooldownGroup= */ false);

        // Call the new method to share enterprise policy check logic.
        TriggerDetails details = shouldTriggerHelpUiWithSnooze(feature, properties);
        if (!details.shouldTriggerIph) {
            return details;
        }
        if (mAggressivelyCooldownIphsSupplier.getAsBoolean()) {
            recordCooldownTriggered(UiFormat.DIALOG);
            recordCooldownTriggered(UiFormat.BLUE_BUBBLE);
        }
        return details;
    }

    @Override
    public TriggerDetails shouldTriggerHelpUiWithSnooze(
            String feature, FeatureProperties featureProperties) {
        return shouldTriggerHelpUiInternal(feature, featureProperties, /* withSnooze= */ true);
    }

    /**
     * Evaluates native feature rules, enterprise policy, and format cooldown groups before
     * triggering the feature in native FET.
     *
     * @param feature The name of the feature requesting in-product help.
     * @param featureProperties The {@link FeatureProperties} for cooldown and enterprise policy
     *     evaluation.
     * @param withSnooze Whether to query native {@code shouldTriggerHelpUiWithSnooze}.
     * @return The {@link TriggerDetails} indicating whether to show the IPH and snooze UI.
     */
    private TriggerDetails shouldTriggerHelpUiInternal(
            String feature, FeatureProperties featureProperties, boolean withSnooze) {
        // Disable all IPH if the UI is in screenshot mode. For context, see crbug.com/964012.
        if (CommandLine.getInstance().hasSwitch(UiSwitches.ENABLE_SCREENSHOT_UI_MODE)) {
            return new TriggerDetails(false, false);
        }

        assert mNativePtr != 0;

        // 1. Feature fails its own rules/groups -> delegate to C++ to log normal failure metrics.
        if (!TrackerImplJni.get().wouldTriggerHelpUi(mNativePtr, feature)) {
            return triggerNative(feature, withSnooze);
        }

        // 2. Enterprise policy check.
        if (featureProperties.isBlockedByEnterprisePolicy()
                && !mPromotionsEnabledSupplier.getAsBoolean()) {
            RecordUserAction.record("InProductHelp.ShouldTriggerHelpUI." + feature);
            RecordUserAction.record(
                    "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered." + feature);
            recordFailureHistograms(
                    feature, TriggerHelpUIResult.FAILURE_BLOCKED_BY_ENTERPRISE_POLICY);
            return new TriggerDetails(false, false);
        }

        @UiFormat int format = featureProperties.getFormat();
        String cooldownFeature = getCooldownFeature(format);
        // If the cooldown feature is null, we don't need to check whether the cooldown feature
        // blocks us OR whether we need to contribute to the cooldown. Skip steps 3 & 4.
        if (cooldownFeature == null) {
            return triggerNative(feature, withSnooze);
        }

        // 3. Cooldown group check.
        // Don't actually call shouldTriggerHelpUi for cooldownFeature, since we don't know whether
        // we need to contribute to the cooldown group.
        if (!TrackerImplJni.get().wouldTriggerHelpUi(mNativePtr, cooldownFeature)) {
            RecordUserAction.record("InProductHelp.CooldownGroupActive." + cooldownFeature);
            if (featureProperties.isBlockedByCooldownGroup()) {
                RecordUserAction.record("InProductHelp.ShouldTriggerHelpUI." + feature);
                RecordUserAction.record(
                        "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered." + feature);
                RecordUserAction.record(
                        "InProductHelp.ShouldTriggerHelpUIResult.BlockedByCooldownGroup."
                                + feature);
                // Reuses FAILURE_OTHER_PRECONDITION_UNMET so every NotTriggered action records a
                // ShouldTriggerHelpUI histogram failure reason; feature-level event_configs
                // failures can be isolated by subtracting BlockedByCooldownGroup.
                recordFailureHistograms(
                        feature, TriggerHelpUIResult.FAILURE_OTHER_PRECONDITION_UNMET);
                return new TriggerDetails(false, false);
            } else {
                // Cooldown wants to block IPHs, but we are not blocked by cooldown group.
                RecordUserAction.record(
                        "InProductHelp.ShouldTriggerHelpUIResult.BypassedCooldownGroup." + feature);
            }
        }

        // 4. If we need to block the cooldown group, notify the cooldown group.
        if (featureProperties.blocksCooldownGroup()) {
            recordCooldownTriggered(format);
        }

        // 5. All checks passed -> trigger feature (C++ logs ShouldTriggerHelpUI + Triggered).
        return triggerNative(feature, withSnooze);
    }

    /**
     * Triggers the feature in native FET via JNI, with or without snooze details.
     *
     * @param feature The name of the feature requesting in-product help.
     * @param withSnooze Whether to call native {@code shouldTriggerHelpUiWithSnooze}.
     * @return The resulting {@link TriggerDetails}.
     */
    private TriggerDetails triggerNative(String feature, boolean withSnooze) {
        if (withSnooze) {
            return TrackerImplJni.get().shouldTriggerHelpUiWithSnooze(mNativePtr, feature);
        }
        return new TriggerDetails(
                TrackerImplJni.get().shouldTriggerHelpUi(mNativePtr, feature), false);
    }

    /**
     * Records {@link TriggerHelpUIResult#FAILURE} and the given {@code reason} to both the
     * per-feature and aggregate shadow {@code InProductHelp.ShouldTriggerHelpUI} histograms
     * (matching native {@code LogTriggerHelpUIResult} in {@code stats.cc}).
     *
     * @param feature The name of the feature that failed to trigger.
     * @param reason The {@link TriggerHelpUIResult} failure reason enum value.
     */
    private static void recordFailureHistograms(String feature, @TriggerHelpUIResult int reason) {
        for (String histogram :
                new String[] {
                    "InProductHelp.ShouldTriggerHelpUI." + feature,
                    "InProductHelp.ShouldTriggerHelpUI"
                }) {
            RecordHistogram.recordEnumeratedHistogram(
                    histogram, TriggerHelpUIResult.FAILURE, TriggerHelpUIResult.COUNT);
            RecordHistogram.recordEnumeratedHistogram(histogram, reason, TriggerHelpUIResult.COUNT);
        }
    }

    /**
     * Notifies the cooldown group for the given {@code format} that an IPH was shown, immediately
     * dismissing the cooldown group feature if triggered in native FET.
     *
     * @param format The {@link UiFormat} whose cooldown group should be updated.
     */
    private void recordCooldownTriggered(@UiFormat int format) {
        String cooldownFeature = getCooldownFeature(format);
        if (cooldownFeature == null) return;
        // Call shouldTriggerHelpUi when possible, because if it "shows", it contributes to the
        // session limit. Because of SessionRateImpact::Type::EXPLICIT affecting only
        // cooldownFeature, it will not prevent other IPHs from showing due to session limit. We
        // want to keep track of the session limit for blue bubbles so that we will know if a blue
        // bubble has already been shown this session.
        if (TrackerImplJni.get().shouldTriggerHelpUi(mNativePtr, cooldownFeature)) {
            TrackerImplJni.get().dismissed(mNativePtr, cooldownFeature);
        } else {
            // Even if cooldownFeature did not "show", we can still record a trigger event for
            // future sessions. This branch only matters if an IPH bypasses the interruptive
            // cooldown.
            String triggerEvent = getCooldownTriggerEvent(format);
            if (triggerEvent != null) {
                notifyEvent(triggerEvent);
            }
        }
    }

    /**
     * Returns the FET cooldown group feature name for the given {@link UiFormat}, or {@code null}
     * if the format does not belong to a cooldown group.
     *
     * @param format The {@link UiFormat} to look up.
     * @return The cooldown group feature name, or {@code null}.
     */
    private static @Nullable String getCooldownFeature(@UiFormat int format) {
        switch (format) {
            case UiFormat.BLUE_BUBBLE:
                return FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP;
            case UiFormat.BOTTOM_SHEET:
            case UiFormat.FRE:
            case UiFormat.ALERTS:
            case UiFormat.DIALOG:
                return FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP;
            case UiFormat.UNKNOWN:
            case UiFormat.SET_UP_LIST:
            case UiFormat.MAGIC_STACK:
            case UiFormat.NOTIFICATIONS:
            case UiFormat.MESSAGES:
            case UiFormat.INLINE_PROMO:
            default:
                return null;
        }
    }

    /**
     * Returns the FET trigger event name for the given {@link UiFormat}'s cooldown group, or {@code
     * null} if the format does not belong to a cooldown group.
     *
     * @param format The {@link UiFormat} to look up.
     * @return The cooldown group trigger event name, or {@code null}.
     */
    private static @Nullable String getCooldownTriggerEvent(@UiFormat int format) {
        switch (format) {
            case UiFormat.BLUE_BUBBLE:
                return EventConstants.BLUE_BUBBLE_COOLDOWN_GROUP_TRIGGER;
            case UiFormat.BOTTOM_SHEET:
            case UiFormat.FRE:
            case UiFormat.ALERTS:
            case UiFormat.DIALOG:
                return EventConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP_TRIGGER;
            case UiFormat.UNKNOWN:
            case UiFormat.SET_UP_LIST:
            case UiFormat.MAGIC_STACK:
            case UiFormat.NOTIFICATIONS:
            case UiFormat.MESSAGES:
            case UiFormat.INLINE_PROMO:
            default:
                return null;
        }
    }

    @Override
    public boolean wouldTriggerHelpUi(String feature) {
        if (!mPromotionsEnabledSupplier.getAsBoolean()) {
            return false;
        }
        assert mNativePtr != 0;
        return TrackerImplJni.get().wouldTriggerHelpUi(mNativePtr, feature);
    }

    @Override
    public boolean hasEverTriggered(String feature, boolean fromWindow) {
        assert mNativePtr != 0;
        return TrackerImplJni.get().hasEverTriggered(mNativePtr, feature, fromWindow);
    }

    @Override
    public void dismissed(String feature) {
        assert mNativePtr != 0;
        TrackerImplJni.get().dismissed(mNativePtr, feature);
    }

    @Override
    public void dismissedWithSnooze(String feature, int snoozeAction) {
        assert mNativePtr != 0;
        TrackerImplJni.get().dismissedWithSnooze(mNativePtr, feature, snoozeAction);
    }

    @Override
    @CheckResult
    public @Nullable DisplayLockHandle acquireDisplayLock() {
        assert mNativePtr != 0;
        return TrackerImplJni.get().acquireDisplayLock(mNativePtr);
    }

    @Override
    public void setPriorityNotification(String feature) {
        TrackerImplJni.get().setPriorityNotification(mNativePtr, feature);
    }

    @Override
    public @Nullable String getPendingPriorityNotification() {
        return TrackerImplJni.get().getPendingPriorityNotification(mNativePtr);
    }

    @Override
    public void registerPriorityNotificationHandler(
            String feature, Runnable priorityNotificationHandler) {
        TrackerImplJni.get()
                .registerPriorityNotificationHandler(
                        mNativePtr, feature, priorityNotificationHandler);
    }

    @Override
    public void unregisterPriorityNotificationHandler(String feature) {
        TrackerImplJni.get().unregisterPriorityNotificationHandler(mNativePtr, feature);
    }

    @Override
    public boolean isInitialized() {
        assert mNativePtr != 0;
        return TrackerImplJni.get().isInitialized(mNativePtr);
    }

    @Override
    public void addOnInitializedCallback(Callback<Boolean> callback) {
        assert mNativePtr != 0;
        TrackerImplJni.get().addOnInitializedCallback(mNativePtr, callback);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativePtr != 0;
        return mNativePtr;
    }

    @CalledByNative
    private static TriggerDetails createTriggerDetails(
            boolean shouldTriggerIph, boolean shouldShowSnooze) {
        return new TriggerDetails(shouldTriggerIph, shouldShowSnooze);
    }

    @NativeMethods
    interface Natives {
        void notifyEvent(long nativeTrackerImplAndroid, String event);

        boolean shouldTriggerHelpUi(long nativeTrackerImplAndroid, String feature);

        TriggerDetails shouldTriggerHelpUiWithSnooze(long nativeTrackerImplAndroid, String feature);

        boolean wouldTriggerHelpUi(long nativeTrackerImplAndroid, String feature);

        boolean hasEverTriggered(long nativeTrackerImplAndroid, String feature, boolean fromWindow);

        void dismissed(long nativeTrackerImplAndroid, String feature);

        void dismissedWithSnooze(long nativeTrackerImplAndroid, String feature, int snoozeAction);

        DisplayLockHandleAndroid acquireDisplayLock(long nativeTrackerImplAndroid);

        void setPriorityNotification(long nativeTrackerImplAndroid, String feature);

        String getPendingPriorityNotification(long nativeTrackerImplAndroid);

        void registerPriorityNotificationHandler(
                long nativeTrackerImplAndroid,
                @JniType("std::string") String feature,
                @JniType("base::OnceClosure") Runnable priorityNotificationHandler);

        void unregisterPriorityNotificationHandler(long nativeTrackerImplAndroid, String feature);

        boolean isInitialized(long nativeTrackerImplAndroid);

        void addOnInitializedCallback(
                long nativeTrackerImplAndroid,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void release(long nativeDisplayLockHandleAndroid);
    }
}
