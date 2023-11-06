// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.view.accessibility.AccessibilityManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

import java.util.List;

@JNINamespace("chromecast")
public final class CastAccessibilityHelper {
    /**
     * Copied from //chrome/android/java/src/org/chromium/chrome/browser/util/AccessibilityUtil.java
     * Checks to see that this device has accessibility and touch exploration enabled.
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    @CalledByNative
    private static boolean isScreenReaderEnabled() {
        AccessibilityManager manager =
                (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);

        if (manager == null) return false;
        if (!manager.isEnabled()) return false;
        if (manager.isTouchExplorationEnabled()) return true;

        List<AccessibilityServiceInfo> services = manager.getEnabledAccessibilityServiceList(
                AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
        for (AccessibilityServiceInfo service : services) {
            if ((service.getCapabilities()
                        & AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES)
                    != 0) {
                return true;
            }
        }
        return false;
    }
}
