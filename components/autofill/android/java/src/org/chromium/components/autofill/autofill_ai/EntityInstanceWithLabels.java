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
    public final String guid;
    // The label to be displayed in the UI surface, for example "Vehicle".
    public final String entityInstanceLabel;
    // The sublabel to be displayed in the UI surface, for example "Fiat".
    public final String entityInstanceSubLabel;
    // Whether the entity is a server or a local entity.
    public final boolean storedInWallet;

    @CalledByNative
    public EntityInstanceWithLabels(
            @JniType("std::string") String guid,
            @JniType("std::u16string") String entityInstanceLabel,
            @JniType("std::u16string") String entityInstanceSubLabel,
            boolean storedInWallet) {
        this.guid = guid;
        this.entityInstanceLabel = entityInstanceLabel;
        this.entityInstanceSubLabel = entityInstanceSubLabel;
        this.storedInWallet = storedInWallet;
    }
}
