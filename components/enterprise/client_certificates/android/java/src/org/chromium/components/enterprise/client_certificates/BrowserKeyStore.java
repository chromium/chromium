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

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.GeneralSecurityException;
import java.security.InvalidAlgorithmParameterException;
import java.security.Key;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.util.ArrayList;
import java.util.List;

/**
 * BrowserKeyStore creates and stores browser keys for matching external credentials.
 *
 * <p>On Android, we rely on the underlying AndroidKeyStore to create and store browser keys. Note
 * that AndroidKeyStore keys are deleted when the Android App (e.g. Chrome) is uninstalled. The keys
 * are created in StrongBox on the device secure element when available.
 */
@JNINamespace("client_certificates")
@NullMarked
public final class BrowserKeyStore {
    private static final String TAG = "ClientCertificates";

    public static final String ANDROID_KEY_STORE = "AndroidKeyStore";

    static final String KEY_LOAD_RESULT_HISTOGRAM =
            "Enterprise.ClientCertificates.ClankKeyLoadResult";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(KeyLoadResult)
    /** The result of loading an existing browser key. Recorded in a histogram. */
    @IntDef({KeyLoadResult.FOUND, KeyLoadResult.NOT_FOUND, KeyLoadResult.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    @interface KeyLoadResult {
        int FOUND = 0;
        int NOT_FOUND = 1;
        int ERROR = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:ClankClientCertificatesKeyLoadResult)

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
     * Creates a new browser key. Any existing key with the same identifier is replaced.
     *
     * @param identifier A new, unique identifier for the browser key.
     * @param allowedAlgorithms The credential parameters the key may be created with. Only ES256 is
     *     supported; no key is created if it is not allowed.
     * @return The BrowserKey object or null when the key pair could not be created.
     */
    @CalledByNative
    public @Nullable @JniType("std::unique_ptr<BrowserKeyAndroid>") BrowserKey
            createBrowserKeyForCredentialId(
                    @JniType("std::vector<uint8_t>") byte[] identifier,
                    List<PublicKeyCredentialParameters> allowedAlgorithms) {
        if (!containsEs256(allowedAlgorithms)) {
            return null;
        }
        return createBrowserKey(identifier, keyStoreAliasOf(identifier));
    }

    /**
     * Gets an existing browser key. Never creates one.
     *
     * @param identifier An identifier for the browser key.
     * @return The BrowserKey object, or null when no key exists for the identifier or the key store
     *     could not be queried.
     */
    @CalledByNative
    public @Nullable @JniType("std::unique_ptr<BrowserKeyAndroid>") BrowserKey
            getBrowserKeyForCredentialId(@JniType("std::vector<uint8_t>") byte[] identifier) {
        BrowserKey browserKey = null;
        @KeyLoadResult int result;
        try {
            browserKey = loadBrowserKey(identifier, keyStoreAliasOf(identifier));
            result = browserKey != null ? KeyLoadResult.FOUND : KeyLoadResult.NOT_FOUND;
        } catch (Exception e) {
            Log.e(TAG, "Could not load the browser key from the key store.", e);
            result = KeyLoadResult.ERROR;
        }
        RecordHistogram.recordEnumeratedHistogram(
                KEY_LOAD_RESULT_HISTOGRAM, result, KeyLoadResult.NUM_ENTRIES);
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
                Log.e(TAG, "The key store could not be loaded while deleting a browser key.", e);
            }
            if (!keyStore.containsAlias(keyStoreAlias)) {
                return;
            }
            keyStore.deleteEntry(keyStoreAlias);
        } catch (KeyStoreException e) {
            Log.e(TAG, "The key store could not delete the browser key.", e);
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

    /**
     * Loads an existing browser key from the key store.
     *
     * @return The BrowserKey object, or null when no key exists for the alias.
     * @throws GeneralSecurityException when the key store could not be queried, in which case the
     *     key may still exist.
     * @throws IOException when the key store could not be loaded.
     * @throws RuntimeException when the AndroidKeyStore provider is in a degraded state.
     */
    private @Nullable BrowserKey loadBrowserKey(byte[] identifier, String keyStoreAlias)
            throws GeneralSecurityException, IOException {
        KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
        keyStore.load(null);
        // containsAlias() and getEntry() report any key store error as a missing entry. On
        // Android 12+, getKey() returns null only when there is no key for the alias and throws
        // UnrecoverableKeyException for other errors; older releases still report some errors as
        // a missing key.
        Key key = keyStore.getKey(keyStoreAlias, /* password= */ null);
        if (key == null) {
            return null;
        }
        if (!(key instanceof PrivateKey)) {
            throw new KeyStoreException(
                    "Key store entry is a "
                            + key.getClass().getName()
                            + " instead of a private key: "
                            + keyStoreAlias);
        }
        Certificate certificate = keyStore.getCertificate(keyStoreAlias);
        if (certificate == null) {
            throw new KeyStoreException(
                    "Key store entry has no certificate holding the public key: " + keyStoreAlias);
        }
        return new BrowserKey(
                identifier, new KeyPair(certificate.getPublicKey(), (PrivateKey) key));
    }

    private @Nullable BrowserKey createBrowserKey(byte[] identifier, String keyStoreAlias) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.M) {
            Log.w(
                    TAG,
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
            Log.e(TAG, "Could not initialize key pair generation for browser key support.", e);
            return null;
        } catch (Exception e) {
            Log.e(TAG, "An unexpected error occurred during key pair generation.", e);
            return null;
        }
    }

    private static @Nullable KeyPairGenerator getAndroidKeyPairGenerator() {
        try {
            return KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, ANDROID_KEY_STORE);
        } catch (NoSuchAlgorithmException | NoSuchProviderException e) {
            Log.e(TAG, "Could not create key pair generation for browser key support.", e);
            return null;
        }
    }
}
