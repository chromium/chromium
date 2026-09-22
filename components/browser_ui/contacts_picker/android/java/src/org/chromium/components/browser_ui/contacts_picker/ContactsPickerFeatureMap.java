// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.os.Build;

import androidx.annotation.ChecksSdkIntAtLeast;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Java accessor for base::Features listed in {@link ContactsPickerFeatureList} */
@JNINamespace("browser_ui")
@NullMarked
public class ContactsPickerFeatureMap extends FeatureMap {
    private static final ContactsPickerFeatureMap sInstance = new ContactsPickerFeatureMap();
    private static @Nullable ContactsPickerFeatureMap sInstanceForTesting;
    private static @Nullable Boolean sSystemContactsPickerEnabledForTesting;

    // Do not instantiate this class.
    protected ContactsPickerFeatureMap() {}

    /**
     * @return the singleton ContactsPickerFeatureMap.
     */
    public static ContactsPickerFeatureMap getInstance() {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return sInstance;
    }

    /**
     * @param instance The instance to use for testing.
     */
    public static void setInstanceForTesting(@Nullable ContactsPickerFeatureMap instance) {
        sInstanceForTesting = instance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    /**
     * Overrides whether the system contacts picker should be used for testing.
     *
     * @param enabled Whether the system contacts picker should be enabled, or null to reset.
     */
    public static void setSystemContactsPickerEnabledForTesting(@Nullable Boolean enabled) {
        sSystemContactsPickerEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sSystemContactsPickerEnabledForTesting = null);
    }

    /** Returns whether the system contacts picker should be used instead of the built-in one. */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.CINNAMON_BUN)
    public static boolean shouldShowSystemContactsPicker() {
        if (sSystemContactsPickerEnabledForTesting != null) {
            return sSystemContactsPickerEnabledForTesting;
        }
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN;
    }

    @Override
    protected long getNativeMap() {
        return ContactsPickerFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
