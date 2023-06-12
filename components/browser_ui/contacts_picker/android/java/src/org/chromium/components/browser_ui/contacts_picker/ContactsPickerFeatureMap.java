// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import org.chromium.base.FeatureMap;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base::Features listed in {@link ContactsPickerFeatureList}
 */
@JNINamespace("browser_ui")
public final class ContactsPickerFeatureMap extends FeatureMap {
    private static final ContactsPickerFeatureMap sInstance = new ContactsPickerFeatureMap();

    // Do not instantiate this class.
    private ContactsPickerFeatureMap() {}

    /**
     * @return the singleton ContactsPickerFeatureMap.
     */
    public static ContactsPickerFeatureMap getInstance() {
        return sInstance;
    }

    /**
     * Convenience method to call {@link #isEnabledInNative(String)} statically.
     */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
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
