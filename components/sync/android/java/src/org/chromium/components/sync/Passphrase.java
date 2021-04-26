// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import java.util.ArrayList;
import java.util.List;

/**
 * This class provides some utility functions to deal with sync passphrases.
 */
public class Passphrase {
    /**
     * Returns whether a passphrase type represents an "explicit" passphrase, which usually means
     * a custom passphrase, but also includes the legacy equivalent, a frozen implicit passphrase.
     */
    private static boolean isExplicitPassphraseType(@PassphraseType int type) {
        switch (type) {
            case PassphraseType.IMPLICIT_PASSPHRASE: // Intentional fall through.
            case PassphraseType.TRUSTED_VAULT_PASSPHRASE: // Intentional fall through.
            case PassphraseType.KEYSTORE_PASSPHRASE:
                return false;
            case PassphraseType.FROZEN_IMPLICIT_PASSPHRASE: // Intentional fall through.
            case PassphraseType.CUSTOM_PASSPHRASE:
                return true;
        }

        return false;
    }

    /**
     * Get the types that are displayed for the current type.
     */
    public static List<Integer /* @Type */> getVisibleTypes(@PassphraseType int type) {
        List<Integer /* @Type */> visibleTypes = new ArrayList<>();
        if (isExplicitPassphraseType(type)) {
            visibleTypes.add(type);
            visibleTypes.add(PassphraseType.KEYSTORE_PASSPHRASE);
        } else {
            visibleTypes.add(PassphraseType.CUSTOM_PASSPHRASE);
            visibleTypes.add(type);
        }
        return visibleTypes;
    }

    /**
     * Get the types that are allowed to be enabled from the current type.
     * @param isCustomPassphraseAllowed Whether setting a custom passphrase is allowed.
     */
    public static List<Integer /* @Type */> getAllowedTypes(
            @PassphraseType int type, boolean isCustomPassphraseAllowed) {
        List<Integer /* @Type */> allowedTypes = new ArrayList<>();
        if (!isExplicitPassphraseType(type)) {
            allowedTypes.add(type);
            if (isCustomPassphraseAllowed) allowedTypes.add(PassphraseType.CUSTOM_PASSPHRASE);
        }
        return allowedTypes;
    }
}
