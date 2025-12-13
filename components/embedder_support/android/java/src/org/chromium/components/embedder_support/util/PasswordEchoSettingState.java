// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.content.ContentResolver;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
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
    // TODO(b/463981764): Add link to developer documentation for the settings.
    private static final String KEY_TEXT_SHOW_PASSWORD_LEGACY = "show_password";
    private static final String KEY_TEXT_SHOW_PASSWORD_PHYSICAL = "show_password_physical";
    private static final String KEY_TEXT_SHOW_PASSWORD_TOUCH = "show_password_touch";

    private boolean mPasswordEchoEnabledLegacy;
    private boolean mPasswordEchoEnabledPhysical;
    private boolean mPasswordEchoEnabledTouch;

    private final SettingObserver mSettingObserver;

    private final boolean mSettingSplitEnabled;
    private static @Nullable Boolean sSplitEnabledForTesting;

    private final ObserverList<PasswordEchoSettingObserver> mObservers = new ObserverList<>();

    private static class LazyHolder {
        static final PasswordEchoSettingState INSTANCE = new PasswordEchoSettingState();
    }

    private boolean isSplitShowPasswordsToTouchAndPhysicalEnabled() {
        if (sSplitEnabledForTesting != null) {
            return sSplitEnabledForTesting;
        }

        // TODO(crbug.com/466343369): Implement the logic to check if the split setting feature is
        // enabled on the platform side.
        return false;
    }

    private static @Nullable PasswordEchoSettingState sInstanceForTests;

    public static void setInstanceForTests(boolean splitEnabled) {
        sSplitEnabledForTesting = splitEnabled;
        sInstanceForTests = new PasswordEchoSettingState();
        ResettersForTesting.register(
                () -> {
                    sSplitEnabledForTesting = null;
                    sInstanceForTests = null;
                });
    }

    private PasswordEchoSettingState() {
        try (TraceEvent e = TraceEvent.scoped("PasswordEchoSettingState.constructor")) {
            final TimeUtils.ElapsedRealtimeNanosTimer timer =
                    new TimeUtils.ElapsedRealtimeNanosTimer();
            mSettingSplitEnabled = isSplitShowPasswordsToTouchAndPhysicalEnabled();

            ContentResolver contentResolver =
                    ContextUtils.getApplicationContext().getContentResolver();
            mSettingObserver = new SettingObserver();

            if (mSettingSplitEnabled) {
                contentResolver.registerContentObserver(
                        Settings.Secure.getUriFor(KEY_TEXT_SHOW_PASSWORD_PHYSICAL),
                        false,
                        mSettingObserver);
                contentResolver.registerContentObserver(
                        Settings.Secure.getUriFor(KEY_TEXT_SHOW_PASSWORD_TOUCH),
                        false,
                        mSettingObserver);
            } else {
                contentResolver.registerContentObserver(
                        Settings.System.getUriFor(KEY_TEXT_SHOW_PASSWORD_LEGACY),
                        false,
                        mSettingObserver);
            }

            updateAllSettingState();
            RecordHistogram.recordMicroTimesHistogram(
                    "Android.PasswordEcho.SettingStateInitializationTime",
                    timer.getElapsedMicros());
        }
    }

    public static PasswordEchoSettingState getInstance() {
        return sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
    }

    private void updateAllSettingState() {
        final TimeUtils.ElapsedRealtimeNanosTimer timer = new TimeUtils.ElapsedRealtimeNanosTimer();
        if (mSettingSplitEnabled) {
            updatePhysicalSettingState();
            updateTouchSettingState();
            RecordHistogram.recordMicroTimesHistogram(
                    "Android.PasswordEcho.SettingReadTime.Split", timer.getElapsedMicros());
        } else {
            updateLegacySettingState();
            RecordHistogram.recordMicroTimesHistogram(
                    "Android.PasswordEcho.SettingReadTime.Legacy", timer.getElapsedMicros());
        }
    }

    private void updateLegacySettingState() {
        mPasswordEchoEnabledLegacy =
                Settings.System.getInt(
                                ContextUtils.getApplicationContext().getContentResolver(),
                                KEY_TEXT_SHOW_PASSWORD_LEGACY,
                                0)
                        == 1;
    }

    private void updatePhysicalSettingState() {
        mPasswordEchoEnabledPhysical =
                Settings.Secure.getInt(
                                ContextUtils.getApplicationContext().getContentResolver(),
                                KEY_TEXT_SHOW_PASSWORD_PHYSICAL,
                                0)
                        == 1;
    }

    private void updateTouchSettingState() {
        mPasswordEchoEnabledTouch =
                Settings.Secure.getInt(
                                ContextUtils.getApplicationContext().getContentResolver(),
                                KEY_TEXT_SHOW_PASSWORD_TOUCH,
                                0)
                        == 1;
    }

    private class SettingObserver extends ContentObserver {
        public SettingObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        @Override
        public void onChange(boolean selfChange, @Nullable Uri uri) {
            if (uri == null) {
                return;
            }

            if (uri.equals(Settings.System.getUriFor(KEY_TEXT_SHOW_PASSWORD_LEGACY))) {
                updateLegacySettingState();
            } else if (uri.equals(Settings.Secure.getUriFor(KEY_TEXT_SHOW_PASSWORD_PHYSICAL))) {
                updatePhysicalSettingState();
            } else if (uri.equals(Settings.Secure.getUriFor(KEY_TEXT_SHOW_PASSWORD_TOUCH))) {
                updateTouchSettingState();
            } else {
                return;
            }

            notifyObservers();
        }
    }

    private void notifyObservers() {
        for (PasswordEchoSettingObserver observer : mObservers) {
            observer.onSettingChange();
        }
    }

    public boolean getPasswordEchoEnabledPhysical() {
        // When the split setting feature flag is disabled, return the legacy setting value.
        return mSettingSplitEnabled ? mPasswordEchoEnabledPhysical : mPasswordEchoEnabledLegacy;
    }

    public boolean getPasswordEchoEnabledTouch() {
        // When the split setting feature flag is disabled, return the legacy setting value.
        return mSettingSplitEnabled ? mPasswordEchoEnabledTouch : mPasswordEchoEnabledLegacy;
    }

    public boolean registerObserver(PasswordEchoSettingObserver observer) {
        ThreadUtils.assertOnUiThread();
        return mObservers.addObserver(observer);
    }

    public boolean unregisterObserver(PasswordEchoSettingObserver observer) {
        ThreadUtils.assertOnUiThread();
        return mObservers.removeObserver(observer);
    }

    @VisibleForTesting
    public ContentObserver getSettingObserver() {
        return mSettingObserver;
    }
}
