// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents information of an Autofill AI entity. Used to display to users details about their
 * entities in the management page.
 */
@JNINamespace("autofill")
@NullMarked
public class EntityInstanceWithLabels {
    // The entity ID.
    private final String mGuid;
    // The label to be displayed in the UI surface, for example "Vehicle".
    private final String mEntityInstanceLabel;
    // The sublabel to be displayed in the UI surface, for example "Fiat".
    private final String mEntityInstanceSubLabel;
    // Whether the entity is a server or a local entity.
    private final boolean mStoredInWallet;

    @CalledByNative
    public EntityInstanceWithLabels(
            @JniType("std::string") String guid,
            @JniType("std::u16string") String entityInstanceLabel,
            @JniType("std::u16string") String entityInstanceSubLabel,
            boolean storedInWallet) {
        mGuid = guid;
        mEntityInstanceLabel = entityInstanceLabel;
        mEntityInstanceSubLabel = entityInstanceSubLabel;
        mStoredInWallet = storedInWallet;
    }

    public String getGuid() {
        return mGuid;
    }

    public String getEntityInstanceLabel() {
        return mEntityInstanceLabel;
    }

    public String getEntityInstanceSubLabel() {
        return mEntityInstanceSubLabel;
    }

    public boolean isStoredInWallet() {
        return mStoredInWallet;
    }
}
