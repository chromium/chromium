// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.client_certificates;

import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.UnrecoverableEntryException;
import java.security.cert.CertificateException;
import java.util.ArrayList;
import java.util.List;

/**
 * BrowserKeyStore creates and stores browser keys for matching external credentials.
 *
 * <p>On Android, we rely on the underlying AndroidKeyStore to create and store browser keys. Note
 * that AndroidKeyStore keys are deleted when the Android App (e.g. Chrome) is uninstalled. The keys
 * will be created in StrongBox on the device secure element.
 */
@JNINamespace("client_certificates")
@NullMarked
public final class BrowserKeyStore {

    public static final String ANDROID_KEY_STORE = "AndroidKeyStore";

    private BrowserKeyStore() {}

    /** Provides an instance of BrowserKeyStore. */
    @CalledByNative
    public static BrowserKeyStore getInstance() {
        return new BrowserKeyStore();
    }

    /**
     * Creates a list of PublicKeyCredentialParameters from types and algorithms.
     *
     * @param types The types as {@link org.chromium.blink.mojom.PublicKeyCrednetialType} constants.
     * @param algorithms The algorithm as identified by COSE Algorithm identifiers. See <a
     *     href="https://www.iana.org/assignments/cose/cose.xhtml#algorithms">COSE Algorithms</a>.
     *     <p>This function aids in JNI conversions.
     */
    @CalledByNative
    public static List<PublicKeyCredentialParameters> createListOfCredentialParameters(
            @JniType("std::vector<int32_t>") int[] types,
            @JniType("std::vector<int32_t>") int[] algorithms) {
        assert types.length == algorithms.length;
        ArrayList<PublicKeyCredentialParameters> list = new ArrayList<>(types.length);
        for (int i = 0; i < types.length; i++) {
            PublicKeyCredentialParameters params = new PublicKeyCredentialParameters();
            params.type = types[i];
            params.algorithmIdentifier = algorithms[i];
            list.add(params);
        }
        return list;
    }

    /**
     * Creates the corresponding KeyStore alias.
     *
     * @param identifier The externally provided identifier.
     */
    private String keyStoreAliasOf(byte[] identifier) {
        return BrowserKey.KEYSTORE_ALIAS_PREFIX
                + Base64.encodeToString(identifier, Base64.URL_SAFE);
    }

    /**
     * Get the corresponding browser key (or creates it).
     *
     * @param identifier An identifier for the browser key.
     * @return The BrowserKey object or null when the key pair could not be created.
     */
    @CalledByNative
    public @Nullable @JniType("std::unique_ptr<BrowserKeyAndroid>") BrowserKey
            getOrCreateBrowserKeyForCredentialId(
                    @JniType("std::vector<uint8_t>") byte[] identifier,
                    List<PublicKeyCredentialParameters> allowedAlgorithms) {
        String keyStoreAlias = keyStoreAliasOf(identifier);
        BrowserKey browserKey = getBrowserKey(identifier, keyStoreAlias);
        if (browserKey == null && containsEs256(allowedAlgorithms)) {
            browserKey = createBrowserKey(identifier, keyStoreAlias);
        }
        return browserKey;
    }

    /** Returns whether StrongBox (hardware) key storage is supported on this device. */
    @CalledByNative
    public static boolean getDeviceSupportsHardwareKeys() {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
    }

    @CalledByNative
    public void deleteBrowserKey(@JniType("std::vector<uint8_t>") byte[] identifier) {
        String keyStoreAlias = keyStoreAliasOf(identifier);
        try {
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
            try {
                keyStore.load(null);
            } catch (IOException | NoSuchAlgorithmException | CertificateException e) {
                Log.e(
                        BrowserKey.TAG,
                        "The key store could not be loaded while deleting a browser key.",
                        e);
            }
            if (!keyStore.containsAlias(keyStoreAlias)) {
                return;
            }
            keyStore.deleteEntry(keyStoreAlias);
        } catch (KeyStoreException e) {
            Log.e(BrowserKey.TAG, "The key store could not delete the browser key.", e);
        }
    }

    private boolean containsEs256(List<PublicKeyCredentialParameters> allowedAlgorithms) {
        for (PublicKeyCredentialParameters params : allowedAlgorithms) {
            if (params.type == PublicKeyCredentialType.PUBLIC_KEY
                    && params.algorithmIdentifier == BrowserKey.COSE_ALGORITHM_ES256) {
                return true;
            }
        }
        return false;
    }

    private @Nullable BrowserKey getBrowserKey(byte[] identifier, String keyStoreAlias) {
        try {
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
            keyStore.load(null);
            if (!keyStore.containsAlias(keyStoreAlias)) {
                return null;
            }
            KeyStore.PrivateKeyEntry privateKeyEntry =
                    (KeyStore.PrivateKeyEntry)
                            keyStore.getEntry(keyStoreAlias, /* protParam= */ null);
            return new BrowserKey(
                    identifier,
                    new KeyPair(
                            privateKeyEntry.getCertificate().getPublicKey(),
                            privateKeyEntry.getPrivateKey()));
        } catch (UnrecoverableEntryException e) {
            // Recreate the key pair by returning null here.
            return null;
        } catch (KeyStoreException
                | CertificateException
                | IOException
                | NoSuchAlgorithmException e) {
            Log.e(BrowserKey.TAG, "Could not load the browser key from the key store.");
            return null;
        }
    }

    private @Nullable BrowserKey createBrowserKey(byte[] identifier, String keyStoreAlias) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.M) {
            Log.w(
                    BrowserKey.TAG,
                    "Android Keystore APIs for key generation are not available on this API"
                        + " level.");
            return null;
        }

        KeyPairGenerator generator = getAndroidKeyPairGenerator();
        if (generator == null) {
            return null;
        }

        KeyGenParameterSpec.Builder specBuilder =
                new KeyGenParameterSpec.Builder(keyStoreAlias, KeyProperties.PURPOSE_SIGN)
                        .setDigests(
                                KeyProperties
                                        .DIGEST_SHA256); // Requires Android 6 (VERSION_CODES.M).

        if (Build.VERSION.SDK_INT >= VERSION_CODES.P && getDeviceSupportsHardwareKeys()) {
            specBuilder.setIsStrongBoxBacked(
                    true); // StrongBox requires Android 9 (VERSION_CODES.P).
        }

        try {
            generator.initialize(specBuilder.build());
            return new BrowserKey(identifier, generator.generateKeyPair());
        } catch (InvalidAlgorithmParameterException e) {
            Log.e(
                    BrowserKey.TAG,
                    "Could not initialize key pair generation for browser key support.",
                    e);
            return null;
        } catch (Exception e) {
            Log.e(BrowserKey.TAG, "An unexpected error occurred during key pair generation.", e);
            return null;
        }
    }

    private static @Nullable KeyPairGenerator getAndroidKeyPairGenerator() {
        try {
            return KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, ANDROID_KEY_STORE);
        } catch (NoSuchAlgorithmException | NoSuchProviderException e) {
            Log.e(
                    BrowserKey.TAG,
                    "Could not create key pair generation for browser key support.",
                    e);
            return null;
        }
    }
}
