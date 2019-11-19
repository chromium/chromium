// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import android.os.Bundle;
import android.os.Looper;
import android.util.Pair;

import com.google.android.gms.iid.InstanceID;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;

/**
 * Fake for InstanceIDWithSubtype. Doesn't hit the network or filesystem (so instance IDs don't
 * survive restarts, and sending messages to tokens via the GCM server won't work).
 */
@JNINamespace("instance_id")
public class FakeInstanceIDWithSubtype extends InstanceIDWithSubtype {
    private String mSubtype;
    private String mId;
    private long mCreationTime;

    /** Map from (subtype + ',' + authorizedEntity + ',' + scope) to token. */
    private Map<String, String> mTokens = new HashMap<>();

    /**
     * Enable this in all InstanceID tests to use this fake instead of hitting the network/disk.
     * @return The previous value.
     */
    @CalledByNative
    public static boolean clearDataAndSetEnabled(boolean enable) {
        synchronized (sSubtypeInstancesLock) {
            sSubtypeInstances.clear();
            boolean wasEnabled = sFakeFactoryForTesting != null;
            if (enable) {
                sFakeFactoryForTesting = new FakeFactory() {
                    @Override
                    public InstanceIDWithSubtype create(String subtype) {
                        return new FakeInstanceIDWithSubtype(subtype);
                    }
                };
            } else {
                sFakeFactoryForTesting = null;
            }
            return wasEnabled;
        }
    }

    /**
     * If exactly one instance of InstanceID exists, and it has exactly one token, this returns
     * the subtype of the InstanceID and the authorizedEntity of the token. Otherwise it throws.
     * If a test fails with no InstanceID or no tokens, it probably means subscribing failed, or
     * that the test subscribed in the wrong way (e.g. a GCM registration rather than an InstanceID
     * token). If a test fails with too many InstanceIDs/tokens, the test subscribed too many times.
     */
    public static Pair<String, String> getSubtypeAndAuthorizedEntityOfOnlyToken() {
        synchronized (sSubtypeInstancesLock) {
            if (sSubtypeInstances.size() != 1) {
                throw new IllegalStateException("Expected exactly one InstanceID, but there are "
                        + sSubtypeInstances.size());
            }
            final String subType = sSubtypeInstances.values().iterator().next().getSubtype();
            return Pair.create(subType, getAuthorizedEntityForSubtype(subType));
        }
    }

    /**
     * If an instanceID exists for subtype, and it has exactly one token, this returns
     * the authorizedEntity of the token. Otherwise it throws.
     */
    public static String getAuthorizedEntityForSubtype(String subtype) {
        synchronized (sSubtypeInstancesLock) {
            FakeInstanceIDWithSubtype iid =
                    (FakeInstanceIDWithSubtype) sSubtypeInstances.get(subtype);
            if (iid == null) {
                throw new IllegalStateException("No subtype instance found for " + subtype);
            }
            if (iid.mTokens.size() != 1) {
                throw new IllegalStateException(
                        "Expected exactly one token, but there are " + iid.mTokens.size());
            }
            return iid.mTokens.keySet().iterator().next().split(",", 3)[1];
        }
    }

    private FakeInstanceIDWithSubtype(String subtype) {
        super(null);
        mSubtype = subtype;

        // The first call to InstanceIDWithSubtype.getInstance calls InstanceID.getInstance which
        // triggers a strict mode violation if it's called on the main thread, by reading from
        // SharedPreferences. Since we can't override those static methods to simulate the strict
        // mode violation in tests, check the thread here (which is only called from getInstance).
        if (Looper.getMainLooper() == Looper.myLooper())
            throw new AssertionError(InstanceID.ERROR_MAIN_THREAD);
    }

    @Override
    public String getSubtype() {
        return mSubtype;
    }

    @Override
    public String getId() {
        // InstanceID.getId sometimes triggers a strict mode violation if it's called on the main
        // thread, by reading from SharedPreferences.
        if (Looper.getMainLooper() == Looper.myLooper())
            throw new AssertionError(InstanceID.ERROR_MAIN_THREAD);

        if (mId == null) {
            mCreationTime = System.currentTimeMillis();
            mId = randomBase64(11 /* length */);
        }
        return mId;
    }

    @Override
    public long getCreationTime() {
        // InstanceID.getCreationTime sometimes triggers a strict mode violation if it's called on
        // the main thread, by reading from SharedPreferences.
        if (Looper.getMainLooper() == Looper.myLooper())
            throw new AssertionError(InstanceID.ERROR_MAIN_THREAD);

        return mCreationTime;
    }

    @Override
    public String getToken(String authorizedEntity, String scope) throws IOException {
        return getToken(authorizedEntity, scope, null);
    }

    @Override
    public String getToken(String authorizedEntity, String scope, Bundle extras)
            throws IOException {
        // InstanceID.getToken enforces this.
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(InstanceID.ERROR_MAIN_THREAD);
        }

        String key = getSubtype() + ',' + authorizedEntity + ',' + scope;
        String token = mTokens.get(key);
        if (token == null) {
            getId();
            token = mId + ':' + randomBase64(140 /* length */);
            mTokens.put(key, token);
        }
        return token;
    }

    @Override
    public void deleteToken(String authorizedEntity, String scope) throws IOException {
        // InstanceID.deleteToken enforces this.
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(InstanceID.ERROR_MAIN_THREAD);
        }

        String key = getSubtype() + ',' + authorizedEntity + ',' + scope;
        mTokens.remove(key);
        // Calling deleteToken causes ID to be generated; can be observed though getCreationTime.
        getId();
    }

    @Override
    public void deleteInstanceID() throws IOException {
        synchronized (sSubtypeInstancesLock) {
            sSubtypeInstances.remove(getSubtype());

            // InstanceID.deleteInstanceID calls InstanceID.deleteToken which enforces this.
            if (Looper.getMainLooper() == Looper.myLooper()) {
                throw new IOException(InstanceID.ERROR_MAIN_THREAD);
            }

            mTokens.clear();
            mCreationTime = 0;
            mId = null;
        }
    }

    /** Returns a random base64url encoded string. */
    private static String randomBase64(int encodedLength) {
        // It would probably make more sense for this method to produce fixed-length plaintext,
        // rather than fixed-length encodings that correspond to variable-length plaintext.
        // But the added randomness helps avoid us depending on the length of tokens GCM gives us.
        final String base64urlAlphabet =
                "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
        Random random = new Random();
        StringBuilder sb = new StringBuilder(encodedLength);
        for (int i = 0; i < encodedLength; i++) {
            int index = random.nextInt(base64urlAlphabet.length());
            sb.append(base64urlAlphabet.charAt(index));
        }
        return sb.toString();
    }
}
