// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.annotation.SuppressLint;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Singleton class for user settings related to font sizes. This class manages the force enable zoom
 * user setting, and the "font scale factor" setting. The font scale factor is equal to the OS-level
 * Android "Settings > Accessibility > Display size and text > Font size" setting. This value was
 * previously boosted using the user's Chrome text scaling setting, but that setting has been
 * removed with the release of Accessibility Page Zoom (11/2024). The OS-level user setting is still
 * read and passed to the Web Preference `font_scale_factor`, which is plumbed to the Blink code as
 * the `accessibility_font_scale_factor`. This is used in the text_autosizer.cc file for sites that
 * still use text-size-adjust. The value is no longer boosted, but future work may boost the value
 * based on the user's zoom setting.
 */
@JNINamespace("browser_ui")
public class FontSizePrefs {

    @SuppressLint("StaticFieldLeak")
    private static FontSizePrefs sFontSizePrefs;

    private final long mFontSizePrefsAndroidPtr;

    private FontSizePrefs(BrowserContextHandle browserContextHandle) {
        mFontSizePrefsAndroidPtr =
                FontSizePrefsJni.get().init(FontSizePrefs.this, browserContextHandle);
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

    /**
     * Sets forceEnableZoom due to a user request (e.g. checking a checkbox). This implicitly sets
     * userSetForceEnableZoom.
     */
    public void setForceEnableZoom(boolean enabled) {
        FontSizePrefsJni.get()
                .setForceEnableZoom(mFontSizePrefsAndroidPtr, FontSizePrefs.this, enabled);
    }

    /** Returns whether forceEnableZoom is enabled. */
    public boolean getForceEnableZoom() {
        return FontSizePrefsJni.get()
                .getForceEnableZoom(mFontSizePrefsAndroidPtr, FontSizePrefs.this);
    }

    /**
     * Updates the fontScaleFactor based on the system-wide font scale.
     *
     * <p>This should be called during application start-up and whenever the system font size
     * changes.
     */
    public void setFontScaleFactor() {
        FontSizePrefsJni.get()
                .setFontScaleFactor(
                        mFontSizePrefsAndroidPtr,
                        FontSizePrefs.this,
                        ContextUtils.getApplicationContext()
                                .getResources()
                                .getConfiguration()
                                .fontScale);
    }

    @NativeMethods
    interface Natives {
        long init(FontSizePrefs caller, BrowserContextHandle browserContextHandle);

        void destroy(long nativeFontSizePrefsAndroid);

        void setFontScaleFactor(
                long nativeFontSizePrefsAndroid, FontSizePrefs caller, float fontScaleFactor);

        boolean getForceEnableZoom(long nativeFontSizePrefsAndroid, FontSizePrefs caller);

        void setForceEnableZoom(
                long nativeFontSizePrefsAndroid, FontSizePrefs caller, boolean enabled);
    }
}
