// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * This class describes the type of passphrase required, if any, to decrypt synced data.
 * It maps the native enum PassphraseType.
 */
public class Passphrase {
    @IntDef({Type.IMPLICIT, Type.KEYSTORE, Type.FROZEN_IMPLICIT, Type.CUSTOM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Update {@link ProfileSyncService#getPassphraseType()} after changing lowest value.
        int IMPLICIT = 0; // GAIA-based passphrase (deprecated).
        int KEYSTORE = 1; // Keystore passphrase.
        int FROZEN_IMPLICIT = 2; // Frozen GAIA passphrase.
        int CUSTOM = 3; // User-provided passphrase.
        int NUM_ENTRIES = 4;
    }

    /**
     * Get the types that are displayed for the current type.
     */
    public static List<Integer /* @Type */> getVisibleTypes(@Type int type) {
        List<Integer /* @Type */> visibleTypes = new ArrayList<>();
        switch (type) {
            case Type.IMPLICIT: // Intentional fall through.
            case Type.KEYSTORE:
                visibleTypes.add(Type.CUSTOM);
                visibleTypes.add(type);
                break;
            case Type.FROZEN_IMPLICIT: // Intentional fall through.
            case Type.CUSTOM:
                visibleTypes.add(type);
                visibleTypes.add(Type.KEYSTORE);
                break;
            default:
                assert false;
        }
        return visibleTypes;
    }

    /**
     * Get the types that are allowed to be enabled from the current type.
     * @param isEncryptEverythingAllowed Whether encrypting all data is allowed.
     */
    public static List<Integer /* @Type */> getAllowedTypes(
            @Type int type, boolean isEncryptEverythingAllowed) {
        List<Integer /* @Type */> allowedTypes = new ArrayList<>();
        if (type == Type.IMPLICIT || type == Type.KEYSTORE) {
            allowedTypes.add(type);
            if (isEncryptEverythingAllowed) allowedTypes.add(Type.CUSTOM);
        }
        return allowedTypes;
    }
}