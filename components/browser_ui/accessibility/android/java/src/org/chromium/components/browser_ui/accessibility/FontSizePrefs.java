// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.prefs.PrefService;

/**
 * Static helpers for user settings related to font sizes. This class manages the "font scale
 * factor" setting. The font scale factor is equal to the OS-level Android "Settings > Accessibility
 * > Display size and text > Font size" setting. This value was previously boosted using the user's
 * Chrome text scaling setting, but that setting has been removed with the release of Accessibility
 * Page Zoom (11/2024). The OS-level user setting is still read and passed to the Web Preference
 * `font_scale_factor`, which is plumbed to the Blink code as the `accessibility_font_scale_factor`.
 * This is used in the text_autosizer.cc file for sites that still use text-size-adjust. The value
 * is no longer boosted, but future work may boost the value based on the user's zoom setting.
 */
@JNINamespace("browser_ui")
@NullMarked
public class FontSizePrefs {
    private FontSizePrefs() {}

    /**
     * Updates the font scale factor stored in `prefService` based on the system-wide font scale.
     *
     * <p>This should be called during application start-up and whenever the system font size
     * changes.
     *
     * @param prefService The profile-scoped {@link PrefService} to write the font scale factor to.
     */
    public static void setFontScaleFactor(PrefService prefService) {
        ThreadUtils.assertOnUiThread();
        FontSizePrefsJni.get()
                .setFontScaleFactor(
                        prefService,
                        ContextUtils.getApplicationContext()
                                .getResources()
                                .getConfiguration()
                                .fontScale);
    }

    @NativeMethods
    interface Natives {
        void setFontScaleFactor(
                @JniType("PrefService*") PrefService prefService, float fontScaleFactor);
    }
}
