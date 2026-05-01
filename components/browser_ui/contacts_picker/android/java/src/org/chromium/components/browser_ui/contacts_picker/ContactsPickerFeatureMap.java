// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Java accessor for base::Features listed in {@link ContactsPickerFeatureList} */
@JNINamespace("browser_ui")
@NullMarked
public class ContactsPickerFeatureMap extends FeatureMap {
    private static final ContactsPickerFeatureMap sInstance = new ContactsPickerFeatureMap();
    private static @Nullable ContactsPickerFeatureMap sInstanceForTesting;

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

    /** Returns whether the system contacts picker should be used instead of the built-in one. */
    public static boolean shouldShowSystemContactsPicker() {
        if (!isEnabled(ContactsPickerFeatureList.ANDROID_SYSTEM_CONTACTS_PICKER)) {
            return false;
        }

        AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        return delegate != null && delegate.isSystemContactsPickerEnabled();
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
