// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.user_prefs;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Helper for retrieving a {@link PrefService} from a {@link BrowserContextHandle}. This class is
 * modeled after the C++ class of the same name.
 */
@JNINamespace("user_prefs")
public class UserPrefs {
    /** Returns the {@link PrefService} associated with the given {@link BrowserContextHandle}. */
    public static PrefService get(BrowserContextHandle browserContextHandle) {
        return UserPrefsJni.get().get(browserContextHandle);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        PrefService get(@NonNull BrowserContextHandle browserContextHandle);
    }
}
