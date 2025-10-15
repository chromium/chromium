// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.user_prefs;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Helper for retrieving a {@link PrefService} from a {@link BrowserContextHandle}. This class is
 * modeled after the C++ class of the same name.
 */
@JNINamespace("user_prefs")
@NullMarked
public class UserPrefs {
    private static @Nullable PrefService sPrefServiceForTesting;

    /** Returns the {@link PrefService} associated with the given {@link BrowserContextHandle}. */
    public static PrefService get(BrowserContextHandle browserContextHandle) {
        if (sPrefServiceForTesting != null) {
            return sPrefServiceForTesting;
        }
        return UserPrefsJni.get().get(browserContextHandle);
    }

    public static void setPrefServiceForTesting(PrefService prefService) {
        sPrefServiceForTesting = prefService;
        ResettersForTesting.register(() -> sPrefServiceForTesting = null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        PrefService get(BrowserContextHandle browserContextHandle);
    }
}
