// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.installedapp;

import android.util.SparseArray;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.security.InvalidKeyException;
import java.security.Key;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

/**
 * Helper class for retrieving a device-unique hash for an Android package name.
 *
 * This is used to counter a potential timing attack against the getInstalledRelatedApps API, by
 * adding a pseudo-random time delay to the query. The delay is a hash of a globally unique
 * identifier for the current browser session, and the package name, which means websites are unable
 * to predict what each user's delay will be, nor compare between apps on a given device.
 *
 * The salt is generated per browser session (not per query, page load, user or device) because it
 * we want it to change "occasionally" -- not too frequently, but sometimes. Each time the salt
 * changes, it gives the site another opportunity to collect data that could be averaged out to
 * cancel out the random noise and find the true timing. So we don't want it changing too often.
 * However, it does need to change periodically: because installing or uninstalling the app creates
 * a noticeable change to the timing of the operation, we need to occasionally change the salt to
 * create plausible deniability (the attacker can't tell the difference between the salt changing
 * and the app being installed/uninstalled). The salt is also updated whenever the cookies are
 * cleared.
 */
class PackageHash {
    // This map stores salts that have been calculated for different browser sessions (i.e. Browser
    // Contexts). A SparseArray is used instead of a HashMap to avoid holding a reference to the key
    // object.
    private static final SparseArray<byte[]> sSaltMap = new SparseArray<byte[]>();

    private static byte[] sGlobalSaltForTesting;

    @VisibleForTesting
    static byte[] getSaltBytes(BrowserContextHandle browserContext) {
        if (sGlobalSaltForTesting != null) return sGlobalSaltForTesting;
        SparseArray<byte[]> saltMap = sSaltMap;
        synchronized (saltMap) {
            byte[] salt = saltMap.get(browserContext.hashCode());
            if (salt != null) return salt;

            salt = new byte[20];
            new SecureRandom().nextBytes(salt);
            saltMap.put(browserContext.hashCode(), salt);
            return salt;
        }
    }

    static void setGlobalSaltForTesting(byte[] salt) {
        sGlobalSaltForTesting = salt;
        ResettersForTesting.register(() -> sGlobalSaltForTesting = null);
    }

    /** Returns a SHA-256 hash of the package name, truncated to a 16-bit integer. */
    static short hashForPackage(String packageName, BrowserContextHandle browserContext) {
        byte[] salt = getSaltBytes(browserContext);
        Mac hasher;
        try {
            hasher = Mac.getInstance("HmacSHA256");
        } catch (NoSuchAlgorithmException e) {
            // Should never happen.
            throw new RuntimeException(e);
        }

        byte[] packageNameBytes = ApiCompatibilityUtils.getBytesUtf8(packageName);

        Key key = new SecretKeySpec(salt, "HmacSHA256");
        try {
            hasher.init(key);
        } catch (InvalidKeyException e) {
            // Should never happen.
            throw new RuntimeException(e);
        }
        byte[] digest = hasher.doFinal(packageNameBytes);
        // Take just the first two bytes of the digest.
        int hash = ((digest[0] & 0xff) << 8) | (digest[1] & 0xff);
        return (short) hash;
    }

    @CalledByNative
    public static void onCookiesDeleted(BrowserContextHandle browserContext) {
        SparseArray<byte[]> saltMap = sSaltMap;
        synchronized (saltMap) {
            saltMap.delete(browserContext.hashCode());
        }
    }
}
