// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

/** This class provides some utility functions to deal with sync passphrases. */
public class Passphrase {
    /**
     * Returns whether a passphrase type represents an "explicit" passphrase, which usually means
     * a custom passphrase, but also includes the legacy equivalent, a frozen implicit passphrase.
     */
    public static boolean isExplicitPassphraseType(@PassphraseType int type) {
        switch (type) {
            case PassphraseType.IMPLICIT_PASSPHRASE: // Intentional fall through.
            case PassphraseType.TRUSTED_VAULT_PASSPHRASE: // Intentional fall through.
            case PassphraseType.KEYSTORE_PASSPHRASE:
                return false;
            case PassphraseType.FROZEN_IMPLICIT_PASSPHRASE: // Intentional fall through.
            case PassphraseType.CUSTOM_PASSPHRASE:
                return true;
        }

        assert false : "Invalid passphrase type";
        return false;
    }
}
