// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ObserverList;
import org.chromium.base.PasswordEchoSettingDelegate;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Password echoing is the process of making the last character typed in a password field
 * momentarily visible before it is replaced by a mask (dot or asterisk). The core Android setting
 * that controls this feature ("Show passwords") is currently being split, requiring separate
 * settings for physical keyboard input and virtual (touch) keyboard input.
 *
 * <p>This class abstracts the complexity of this split, allowing consumers to reliably check and
 * observe the correct setting change (either the single legacy setting or the two new split
 * settings) based on the relevant feature flag status.
 *
 * <p>This Singleton class monitors the relevant system setting(s) based on the feature flag status
 * and notifies the observers of relevant setting changes.
 *
 * <ul>
 *   <li>When the split setting flag is disabled, the legacy setting (`show_password`) is monitored.
 *   <li>When the split setting flag is enabled, the new split settings (`show_password_physical`
 *       and `show_password_touch`) are monitored.
 * </ul>
 *
 * <p>The class provides two getters, one for the physical setting
 * (getPasswordEchoEnabledPhysical()) and one for the touch setting (getPasswordEchoEnabledTouch()).
 * Both getters return the value of the legacy setting when the split setting flag is disabled, but
 * they return their respective split setting values when the flag is enabled.
 */
@NullMarked
public class PasswordEchoSettingState {
    private boolean mPasswordEchoEnabledPhysical;
    private boolean mPasswordEchoEnabledTouch;

    private final PasswordEchoSettingDelegate mPasswordEchoSettingDelegate;
    private static @Nullable PasswordEchoSettingDelegate sPasswordEchoSettingDelegateForTesting;

    private final ObserverList<PasswordEchoSettingObserver> mObservers = new ObserverList<>();

    private static class LazyHolder {
        static final PasswordEchoSettingState INSTANCE = new PasswordEchoSettingState();
    }

    private static @Nullable PasswordEchoSettingState sInstanceForTests;

    public static void setInstanceForTests(PasswordEchoSettingDelegate delegate) {
        sPasswordEchoSettingDelegateForTesting = delegate;
        sInstanceForTests = new PasswordEchoSettingState();
        ResettersForTesting.register(
                () -> {
                    sPasswordEchoSettingDelegateForTesting = null;
                    sInstanceForTests = null;
                });
    }

    private PasswordEchoSettingDelegate getPasswordEchoSettingDelegate() {
        if (sPasswordEchoSettingDelegateForTesting != null) {
            return sPasswordEchoSettingDelegateForTesting;
        }

        AconfigFlaggedApiDelegate aconfigDelegate = AconfigFlaggedApiDelegate.getInstance();
        PasswordEchoSettingDelegate delegate =
                aconfigDelegate != null ? aconfigDelegate.getPasswordEchoSettingDelegate() : null;
        return delegate != null ? delegate : new LegacyPasswordEchoSettingDelegateImpl();
    }

    private PasswordEchoSettingState() {
        try (TraceEvent e = TraceEvent.scoped("PasswordEchoSettingState.constructor")) {
            final TimeUtils.ElapsedRealtimeNanosTimer timer =
                    new TimeUtils.ElapsedRealtimeNanosTimer();

            mPasswordEchoSettingDelegate = getPasswordEchoSettingDelegate();

            mPasswordEchoSettingDelegate.registerCallback(this::updateCacheAndNotifyObservers);
            updateCachedSettings();

            RecordHistogram.recordMicroTimesHistogram(
                    "Android.PasswordEcho.SettingStateInitializationTime",
                    timer.getElapsedMicros());
        }
    }

    public static PasswordEchoSettingState getInstance() {
        return sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
    }

    private void updateCachedSettings() {
        final TimeUtils.ElapsedRealtimeNanosTimer timer = new TimeUtils.ElapsedRealtimeNanosTimer();

        mPasswordEchoEnabledPhysical = mPasswordEchoSettingDelegate.isPhysicalSettingEnabled();
        mPasswordEchoEnabledTouch = mPasswordEchoSettingDelegate.isTouchSettingEnabled();

        String suffix =
                mPasswordEchoSettingDelegate instanceof LegacyPasswordEchoSettingDelegateImpl
                        ? "Legacy"
                        : "Split";
        RecordHistogram.recordMicroTimesHistogram(
                "Android.PasswordEcho.SettingReadTime." + suffix, timer.getElapsedMicros());
    }

    public void updateCacheAndNotifyObservers() {
        updateCachedSettings();
        notifyObservers();
    }

    private void notifyObservers() {
        for (PasswordEchoSettingObserver observer : mObservers) {
            observer.onSettingChange();
        }
    }

    public boolean getPasswordEchoEnabledPhysical() {
        return mPasswordEchoEnabledPhysical;
    }

    public boolean getPasswordEchoEnabledTouch() {
        return mPasswordEchoEnabledTouch;
    }

    public boolean registerObserver(PasswordEchoSettingObserver observer) {
        ThreadUtils.assertOnUiThread();
        return mObservers.addObserver(observer);
    }

    public boolean unregisterObserver(PasswordEchoSettingObserver observer) {
        ThreadUtils.assertOnUiThread();
        return mObservers.removeObserver(observer);
    }
}
