// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.annotation.SuppressLint;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Singleton class for accessing these font size-related preferences:
 *  - User Font Scale Factor: the font scale value that the user sees and can set. This is a value
 *        between 50% and 200% (i.e. 0.5 and 2).
 *  - Font Scale Factor: the font scale factor applied to webpage text during font boosting. This
 *        equals the user font scale factor times the Android system font scale factor, which
 *        reflects the font size indicated in Android settings > Display > Font size.
 *  - Force Enable Zoom: whether force enable zoom is on or off
 *  - User Set Force Enable Zoom: whether the user has manually set the force enable zoom button
 */
@JNINamespace("browser_ui")
public class FontSizePrefs {
    /**
     * The font scale threshold beyond which force enable zoom is automatically turned on. It
     * is chosen such that force enable zoom will be activated when the accessibility large text
     * setting is on (i.e. this value should be the same as or lesser than the font size scale used
     * by accessibility large text).
     */
    public static final float FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER = 1.3f;

    @VisibleForTesting
    public static final String FONT_SIZE_CHANGE_HISTOGRAM =
            "Accessibility.Android.UserFontSizePref.Change";

    private static final String FONT_SIZE_STARTUP_HISTOGRAM =
            "Accessibility.Android.UserFontSizePref.OnStartup";

    private static final float EPSILON = 0.001f;

    @SuppressLint("StaticFieldLeak")
    private static FontSizePrefs sFontSizePrefs;

    private final long mFontSizePrefsAndroidPtr;
    private final ObserverList<FontSizePrefsObserver> mObserverList;

    private Float mSystemFontScaleForTests;

    /** Interface for observing changes in font size-related preferences. */
    public interface FontSizePrefsObserver {
        void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor);

        void onForceEnableZoomChanged(boolean enabled);
    }

    private FontSizePrefs(BrowserContextHandle browserContextHandle) {
        mFontSizePrefsAndroidPtr =
                FontSizePrefsJni.get().init(FontSizePrefs.this, browserContextHandle);
        mObserverList = new ObserverList<FontSizePrefsObserver>();
    }

    /** Returns the singleton FontSizePrefs, constructing it if it doesn't already exist. */
    public static FontSizePrefs getInstance(BrowserContextHandle browserContextHandle) {
        ThreadUtils.assertOnUiThread();
        if (sFontSizePrefs == null) {
            sFontSizePrefs = new FontSizePrefs(browserContextHandle);
        }
        return sFontSizePrefs;
    }

    /** Destroys the instance of FontSizePrefs if there is one. */
    public static void destroyInstance() {
        if (sFontSizePrefs == null) {
            return;
        }

        FontSizePrefsJni.get().destroy(sFontSizePrefs.mFontSizePrefsAndroidPtr);
        sFontSizePrefs = null;
    }

    /** Adds an observer to listen for changes to font scale-related preferences. */
    public void addObserver(FontSizePrefsObserver observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Removes an observer so it will no longer receive updates for changes to font scale-related
     * preferences.
     */
    public void removeObserver(FontSizePrefsObserver observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * Updates the fontScaleFactor based on the userFontScaleFactor and the system-wide font scale.
     *
     * This should be called during application start-up and whenever the system font size changes.
     */
    public void onSystemFontScaleChanged() {
        float userFontScaleFactor = getUserFontScaleFactor();
        if (userFontScaleFactor != 0f) {
            setFontScaleFactor(userFontScaleFactor * getSystemFontScale());
        }
    }

    /** Sets the userFontScaleFactor. This should be a value between .5 and 2. */
    public void setUserFontScaleFactor(float userFontScaleFactor) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putFloat(AccessibilityConstants.FONT_USER_FONT_SCALE_FACTOR, userFontScaleFactor)
                .apply();
        setFontScaleFactor(userFontScaleFactor * getSystemFontScale());
    }

    /** Returns the userFontScaleFactor. This is the value that should be displayed to the user. */
    public float getUserFontScaleFactor() {
        float userFontScaleFactor;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            userFontScaleFactor =
                    ContextUtils.getAppSharedPreferences()
                            .getFloat(AccessibilityConstants.FONT_USER_FONT_SCALE_FACTOR, 0f);
        }
        if (userFontScaleFactor == 0f) {
            float fontScaleFactor = getFontScaleFactor();

            if (Math.abs(fontScaleFactor - 1f) <= EPSILON) {
                // If the font scale factor is 1, assume that the user hasn't customized their font
                // scale and/or wants the default value
                userFontScaleFactor = 1f;
            } else {
                // Initialize userFontScaleFactor based on fontScaleFactor, since
                // userFontScaleFactor was added long after fontScaleFactor.
                userFontScaleFactor =
                        MathUtils.clamp(fontScaleFactor / getSystemFontScale(), 0.5f, 2f);
            }
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putFloat(
                            AccessibilityConstants.FONT_USER_FONT_SCALE_FACTOR, userFontScaleFactor)
                    .apply();
        }
        return userFontScaleFactor;
    }

    /**
     * Returns the fontScaleFactor. This is the product of the userFontScaleFactor and the system
     * font scale, and is the amount by which webpage text will be scaled during font boosting.
     */
    public float getFontScaleFactor() {
        return FontSizePrefsJni.get()
                .getFontScaleFactor(mFontSizePrefsAndroidPtr, FontSizePrefs.this);
    }

    /**
     * Sets forceEnableZoom due to a user request (e.g. checking a checkbox). This implicitly sets
     * userSetForceEnableZoom.
     */
    public void setForceEnableZoomFromUser(boolean enabled) {
        setForceEnableZoom(enabled, true);
    }

    /** Returns whether forceEnableZoom is enabled. */
    public boolean getForceEnableZoom() {
        return FontSizePrefsJni.get()
                .getForceEnableZoom(mFontSizePrefsAndroidPtr, FontSizePrefs.this);
    }

    /** Record the user font setting. Intended to be logged on activity startup. */
    public void recordUserFontPrefOnStartup() {
        recordUserFontPrefHistogram(FONT_SIZE_STARTUP_HISTOGRAM);
    }

    /** Record the user font setting when the setting is changed by the user. */
    public void recordUserFontPrefChange() {
        recordUserFontPrefHistogram(FONT_SIZE_CHANGE_HISTOGRAM);
    }

    private void recordUserFontPrefHistogram(String histogramName) {
        // User font size prefs range from 0.5 to 2.0 (50% to 200%) and can be updated in increments
        // of 5% (see org.chromium.chrome.browser.accessibility.settings.TextScalePreference).
        int sample = (int) (getUserFontScaleFactor() * 100);
        assert sample >= 50 && sample <= 200 : "Unexpected font size pref";
        RecordHistogram.recordSparseHistogram(histogramName, sample);
    }

    /** Sets a mock value for the system-wide font scale. Use only in tests. */
    public void setSystemFontScaleForTest(float fontScale) {
        mSystemFontScaleForTests = fontScale;
        ResettersForTesting.register(() -> mSystemFontScaleForTests = null);
    }

    private float getSystemFontScale() {
        if (mSystemFontScaleForTests != null) return mSystemFontScaleForTests;
        return ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale;
    }

    private void setForceEnableZoom(boolean enabled, boolean fromUser) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AccessibilityConstants.FONT_USER_SET_FORCE_ENABLE_ZOOM, fromUser)
                .apply();
        FontSizePrefsJni.get()
                .setForceEnableZoom(mFontSizePrefsAndroidPtr, FontSizePrefs.this, enabled);
    }

    private boolean getUserSetForceEnableZoom() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences()
                    .getBoolean(AccessibilityConstants.FONT_USER_SET_FORCE_ENABLE_ZOOM, false);
        }
    }

    private void setFontScaleFactor(float fontScaleFactor) {
        float previousFontScaleFactor = getFontScaleFactor();
        FontSizePrefsJni.get()
                .setFontScaleFactor(mFontSizePrefsAndroidPtr, FontSizePrefs.this, fontScaleFactor);

        if (previousFontScaleFactor < FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER
                && fontScaleFactor >= FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER
                && !getForceEnableZoom()) {
            // If the font scale factor just crossed above the threshold, set force enable zoom even
            // if the user has previously unset it.
            setForceEnableZoom(true, false);
        } else if (previousFontScaleFactor >= FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER
                && fontScaleFactor < FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER
                && !getUserSetForceEnableZoom()) {
            // If the font scale factor just crossed below the threshold and the user didn't set
            // force enable zoom manually, then unset force enable zoom.
            setForceEnableZoom(false, false);
        }
    }

    @CalledByNative
    private void onFontScaleFactorChanged(float fontScaleFactor) {
        float userFontScaleFactor = getUserFontScaleFactor();
        for (FontSizePrefsObserver observer : mObserverList) {
            observer.onFontScaleFactorChanged(fontScaleFactor, userFontScaleFactor);
        }
    }

    @CalledByNative
    private void onForceEnableZoomChanged(boolean enabled) {
        for (FontSizePrefsObserver observer : mObserverList) {
            observer.onForceEnableZoomChanged(enabled);
        }
    }

    @NativeMethods
    interface Natives {
        long init(FontSizePrefs caller, BrowserContextHandle browserContextHandle);

        void destroy(long nativeFontSizePrefsAndroid);

        void setFontScaleFactor(
                long nativeFontSizePrefsAndroid, FontSizePrefs caller, float fontScaleFactor);

        float getFontScaleFactor(long nativeFontSizePrefsAndroid, FontSizePrefs caller);

        boolean getForceEnableZoom(long nativeFontSizePrefsAndroid, FontSizePrefs caller);

        void setForceEnableZoom(
                long nativeFontSizePrefsAndroid, FontSizePrefs caller, boolean enabled);
    }
}
