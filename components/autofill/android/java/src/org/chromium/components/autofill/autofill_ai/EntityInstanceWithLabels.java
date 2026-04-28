// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Represents information of an Autofill AI entity. Used to display to users details about their
 * entities in the management page.
 */
@JNINamespace("autofill")
@NullMarked
public class EntityInstanceWithLabels {
    // The entity ID.
    private final String mGuid;
    // The type of the entity.
    private final EntityType mType;
    // The label to be displayed in the UI surface, for example "Vehicle".
    private final String mEntityInstanceLabel;
    // The sublabel to be displayed in the UI surface, for example "Fiat".
    private final String mEntityInstanceSubLabel;
    // Whether the entity is a server or a local entity.
    private final boolean mStoredInWallet;
    // The URL to manage the entity in the Wallet application, if it is stored in Wallet.
    private final @Nullable String mWalletEntityUrl;

    @CalledByNative
    public EntityInstanceWithLabels(
            @JniType("std::string") String guid,
            @JniType("autofill::EntityTypeAndroid") EntityType entityType,
            @JniType("std::u16string") String entityInstanceLabel,
            @JniType("std::u16string") String entityInstanceSubLabel,
            boolean storedInWallet,
            @JniType("std::optional<std::string>") @Nullable String walletEntityUrl) {
        mGuid = guid;
        mType = entityType;
        mEntityInstanceLabel = entityInstanceLabel;
        mEntityInstanceSubLabel = entityInstanceSubLabel;
        mStoredInWallet = storedInWallet;
        mWalletEntityUrl = walletEntityUrl;
    }

    @CalledByNative
    public @JniType("std::string") String getGuid() {
        return mGuid;
    }

    @CalledByNative
    public @JniType("std::u16string") String getEntityInstanceLabel() {
        return mEntityInstanceLabel;
    }

    @CalledByNative
    public @JniType("autofill::EntityTypeAndroid") EntityType getEntityType() {
        return mType;
    }

    @CalledByNative
    public @JniType("std::u16string") String getEntityInstanceSubLabel() {
        return mEntityInstanceSubLabel;
    }

    @CalledByNative
    public boolean isStoredInWallet() {
        return mStoredInWallet;
    }

    @CalledByNative
    public @JniType("std::optional<std::string>") @Nullable String getWalletEntityUrl() {
        return mWalletEntityUrl;
    }
}
