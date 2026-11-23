// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyInfo;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.payments.PaymentFeatureList;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.InvalidAlgorithmParameterException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.PrivateKey;
import java.security.ProviderException;
import java.security.UnrecoverableEntryException;
import java.security.cert.CertificateException;
import java.security.spec.InvalidKeySpecException;
import java.util.ArrayList;
import java.util.List;

/**
 * BrowserBoundKeyStore creates and stores browser bound keys for matching external credentials.
 *
 * <p>On Android, we rely on the underlying AndroidKeyStore to create and store browser bound keys.
 * Note that AndroidKeyStore keys are deleted when the Android App (e.g. Chrome) is uninstalled. The
 * keys will be created in StrongBox on the device secure element, or in a Trusted Execution
 * Environment (TEE) on devices without StrongBox.
 */
@JNINamespace("payments")
@NullMarked
public final class BrowserBoundKeyStore {

    /** The logging tag for this class. */
    private static final String TAG = "SpcBbKeyStore";

    public static final String ANDROID_KEY_STORE = "AndroidKeyStore";
    public static final String KEYSTORE_ALIAS_PREFIX = "spcbbk_sha256ecdsa_";

    // The outcome of a browser bound key creation attempt, including the storage used for the key.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(SecurePaymentConfirmationBrowserBoundKeyAndroidCreateKeyResult)
    @IntDef({
        CreateKeyResult.STRONG_BOX,
        CreateKeyResult.TRUSTED_ENVIRONMENT,
        CreateKeyResult.UNKNOWN_SECURE,
        CreateKeyResult.SOFTWARE,
        CreateKeyResult.KEY_INFO_UNAVAILABLE,
        CreateKeyResult.UNEXPECTED_SECURITY_LEVEL,
        CreateKeyResult.STRONG_BOX_CREATION_FAILED,
        CreateKeyResult.NON_STRONG_BOX_CREATION_FAILED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface CreateKeyResult {
        int STRONG_BOX = 0;
        int TRUSTED_ENVIRONMENT = 1;
        int UNKNOWN_SECURE = 2;
        int SOFTWARE = 3;
        int KEY_INFO_UNAVAILABLE = 4;
        int UNEXPECTED_SECURITY_LEVEL = 5;
        int STRONG_BOX_CREATION_FAILED = 6;
        int NON_STRONG_BOX_CREATION_FAILED = 7;
        int NUM_ENTRIES = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/payment/enums.xml:SecurePaymentConfirmationBrowserBoundKeyAndroidCreateKeyResult)

    private static final String CREATE_KEY_RESULT_HISTOGRAM =
            "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStore.Android.CreateKeyResult";

    private static boolean sAllowSoftwareKeysForTesting;

    private BrowserBoundKeyStore() {}

    /** Provides an instance of BrowserBoundKeyStore. */
    @CalledByNative
    public static BrowserBoundKeyStore getInstance() {
        return new BrowserBoundKeyStore();
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
     * Get the corresponding browser bound key (or creates it).
     *
     * @param identifier An identifier for the browser bound key.
     * @param allowedAlgorithms A list of allowed credential parameters used if the key needs to be
     *     created. Consequently, if this list is empty no key will be created but an existing one
     *     may be returned.
     * @return The BrowserBoundKey object or null when the key pair could not be created.
     */
    @CalledByNative
    public @Nullable @JniType("std::unique_ptr<BrowserBoundKeyAndroid>") BrowserBoundKey
            getOrCreateBrowserBoundKeyForCredentialId(
                    @JniType("std::vector<uint8_t>") byte[] identifier,
                    List<PublicKeyCredentialParameters> allowedAlgorithms) {
        if (!getDeviceSupportsHardwareKeys()) {
            // Don't create or return keys unless hardware-backed storage is allowed. In
            // particular, this stops previously-created TEE keys being used if
            // SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE is rolled back.
            return null;
        }

        // TODO(crbug.com/377278827): Generate a random alias and store the association in a table,
        // so that browser bound public keys can be included in clientDataJson on passkey creation
        // time when the identifier is not know.
        String keyStoreAlias = keyStoreAliasOf(identifier);
        BrowserBoundKey browserBoundKey = getBrowserBoundKey(identifier, keyStoreAlias);
        if (browserBoundKey == null && containsEs256(allowedAlgorithms)) {
            browserBoundKey = createBrowserBoundKey(identifier, keyStoreAlias);
        }
        return browserBoundKey;
    }

    /**
     * Delete the browser bound key from the platform key store.
     *
     * <p>If the the underlying delete operation does not succeed the key may still be present.
     *
     * @param identifier The identifier from {@link BrowserBoundKey#GetIdentifier()}.
     */
    @CalledByNative
    public void deleteBrowserBoundKey(@JniType("std::vector<uint8_t>") byte[] identifier) {
        String keyStoreAlias = keyStoreAliasOf(identifier);
        try {
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
            try {
                keyStore.load(null);
            } catch (IOException | NoSuchAlgorithmException | CertificateException e) {
                // Cannot delete when the keystore is not functioning, though none of these
                // exceptions are expected from the AndroidKeyStore implementation.
                Log.e(
                        TAG,
                        "The key store could not be loaded while deleting a browser bound key.",
                        e);
            }
            if (!keyStore.containsAlias(keyStoreAlias)) {
                // If the key does not exist, there is no need to delete it.
                return;
            }
            keyStore.deleteEntry(keyStoreAlias);
        } catch (KeyStoreException e) {
            // There is an unrecoverable exception from the Android KeyStore which has attempted
            // retries if appropriate. (e.g. The keystore daemon did not respond).
            Log.e(TAG, "The key store could not delete the browser bound key.", e);
        }
    }

    /**
     * Returns whether hardware key storage (StrongBox or Trusted Execution Environment) is
     * supported on this device.
     *
     * <p>This only checks the declared system features, so it can return true on devices that
     * misdeclare FEATURE_HARDWARE_KEYSTORE (e.g. emulators), where key creation then fails.
     */
    @CalledByNative
    public static boolean getDeviceSupportsHardwareKeys() {
        if (hasStrongBox()) {
            return true;
        }
        return hasHardwareKeyStore()
                && PaymentFeatureList.isEnabled(
                        PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);
    }

    private static boolean hasStrongBox() {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
    }

    private static boolean hasHardwareKeyStore() {
        return Build.VERSION.SDK_INT >= VERSION_CODES.S
                && ContextUtils.getApplicationContext()
                        .getPackageManager()
                        .hasSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE);
    }

    /**
     * Creates the corresponding KeyStore alias.
     *
     * @param identifier The externally provided identifier.
     */
    private String keyStoreAliasOf(byte[] identifier) {
        return KEYSTORE_ALIAS_PREFIX + Base64.encodeToString(identifier, Base64.URL_SAFE);
    }

    private boolean containsEs256(List<PublicKeyCredentialParameters> allowedAlgorithms) {
        for (PublicKeyCredentialParameters params : allowedAlgorithms) {
            if (params.type == PublicKeyCredentialType.PUBLIC_KEY
                    && params.algorithmIdentifier == BrowserBoundKey.COSE_ALGORITHM_ES256) {
                return true;
            }
        }
        return false;
    }

    private @Nullable BrowserBoundKey getBrowserBoundKey(byte[] identifier, String keyStoreAlias) {
        try {
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
            keyStore.load(null);
            if (!keyStore.containsAlias(keyStoreAlias)) {
                return null;
            }
            KeyStore.PrivateKeyEntry privateKeyEntry =
                    (KeyStore.PrivateKeyEntry)
                            keyStore.getEntry(keyStoreAlias, /* protParam= */ null);
            return new BrowserBoundKey(
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
            // TODO(crbug.com/377278827): Return a reason for the failure instead of recreating the
            // key.
            Log.e(TAG, "Could not load the browser bound key from the key store.");
            return null;
        }
    }

    private @Nullable BrowserBoundKey createBrowserBoundKey(
            byte[] identifier, String keyStoreAlias) {
        KeyPairGenerator generator = getAndroidKeyPairGenerator();
        if (generator == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT < VERSION_CODES.P) {
            return null;
        }

        // getOrCreateBrowserBoundKeyForCredentialId() has already checked
        // getDeviceSupportsHardwareKeys(), so we know that some hardware store is available.
        // The key is created in StrongBox when the device has it, and in the TEE otherwise. There
        // is intentionally no fallback from StrongBox to TEE if a StrongBox create fails, in
        // order to support clean rollback of SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE if
        // needed.
        boolean useStrongBox = hasStrongBox();
        KeyPair keyPair;
        try {
            generator.initialize(
                    new KeyGenParameterSpec.Builder(keyStoreAlias, KeyProperties.PURPOSE_SIGN)
                            .setDigests(KeyProperties.DIGEST_SHA256)
                            .setIsStrongBoxBacked(useStrongBox) // Requires Android 9 (P).
                            .build());
            keyPair = generator.generateKeyPair();
        } catch (InvalidAlgorithmParameterException | ProviderException e) {
            // StrongBoxUnavailableException is a ProviderException.
            // TODO(crbug.com/377278827): Return a reason for the failure.
            Log.e(TAG, "Could not create a browser bound key (StrongBox: %s).", useStrongBox, e);
            recordCreateKeyResult(
                    useStrongBox
                            ? CreateKeyResult.STRONG_BOX_CREATION_FAILED
                            : CreateKeyResult.NON_STRONG_BOX_CREATION_FAILED);
            return null;
        }

        if (useStrongBox) {
            // StrongBox requests fail rather than silently producing a non-StrongBox key, so there
            // is no need to read the key's security level.
            recordCreateKeyResult(CreateKeyResult.STRONG_BOX);
            return new BrowserBoundKey(identifier, keyPair);
        }

        // Non-StrongBox requests can silently produce a software key on devices that misdeclare
        // FEATURE_HARDWARE_KEYSTORE, such as software emulators.
        @CreateKeyResult int result = getNonStrongBoxCreateKeyResult(keyPair.getPrivate());
        recordCreateKeyResult(result);
        boolean isAcceptable =
                result == CreateKeyResult.TRUSTED_ENVIRONMENT
                        || result == CreateKeyResult.UNKNOWN_SECURE
                        || (result == CreateKeyResult.SOFTWARE
                                && BuildConfig.IS_FOR_TEST
                                && sAllowSoftwareKeysForTesting);
        if (!isAcceptable) {
            Log.e(TAG, "The browser bound key is not backed by a hardware TEE.");
            deleteBrowserBoundKey(identifier);
            return null;
        }

        return new BrowserBoundKey(identifier, keyPair);
    }

    private static void recordCreateKeyResult(@CreateKeyResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                CREATE_KEY_RESULT_HISTOGRAM, result, CreateKeyResult.NUM_ENTRIES);
    }

    /**
     * Returns the creation result for a {@code privateKey} that was requested without StrongBox,
     * based on the key's reported security level.
     */
    private static @CreateKeyResult int getNonStrongBoxCreateKeyResult(PrivateKey privateKey) {
        // The TEE path requires Android S (see hasHardwareKeyStore()); this check is for lint.
        if (Build.VERSION.SDK_INT < VERSION_CODES.S) {
            return CreateKeyResult.KEY_INFO_UNAVAILABLE;
        }

        int securityLevel;
        try {
            KeyInfo keyInfo =
                    KeyFactory.getInstance(privateKey.getAlgorithm(), ANDROID_KEY_STORE)
                            .getKeySpec(privateKey, KeyInfo.class);
            securityLevel = keyInfo.getSecurityLevel();
        } catch (NoSuchAlgorithmException
                | NoSuchProviderException
                | InvalidKeySpecException
                | ProviderException e) {
            Log.e(TAG, "Could not read the browser bound key's security level.", e);
            return CreateKeyResult.KEY_INFO_UNAVAILABLE;
        }
        @CreateKeyResult int result = toCreateKeyResult(securityLevel);
        // Android never places a key requested without StrongBox in StrongBox.
        assert result != CreateKeyResult.STRONG_BOX;
        return result;
    }

    /**
     * Maps the {@link KeyInfo#getSecurityLevel()} of a key that was requested without StrongBox to
     * its creation result.
     */
    @RequiresApi(VERSION_CODES.S)
    static @CreateKeyResult int toCreateKeyResult(int securityLevel) {
        switch (securityLevel) {
            case KeyProperties.SECURITY_LEVEL_STRONGBOX:
                return CreateKeyResult.STRONG_BOX;
            case KeyProperties.SECURITY_LEVEL_TRUSTED_ENVIRONMENT:
                return CreateKeyResult.TRUSTED_ENVIRONMENT;
            case KeyProperties.SECURITY_LEVEL_UNKNOWN_SECURE:
                // KeyStore reports UNKNOWN_SECURE when it knows the key is in secure hardware (e.g.
                // from a legacy Keymaster HAL or a key upgraded from pre-Android S) but cannot
                // distinguish TEE from StrongBox, which still satisfies our hardware requirement.
                return CreateKeyResult.UNKNOWN_SECURE;
            case KeyProperties.SECURITY_LEVEL_SOFTWARE:
                return CreateKeyResult.SOFTWARE;
            default:
                return CreateKeyResult.UNEXPECTED_SECURITY_LEVEL;
        }
    }

    private static @Nullable KeyPairGenerator getAndroidKeyPairGenerator() {
        try {
            return KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, ANDROID_KEY_STORE);
        } catch (NoSuchAlgorithmException | NoSuchProviderException e) {
            // TODO(crbug.com/377278827): Return a reason for the failure.
            Log.e(TAG, "Could not create key pair generation for browser bound key support.", e);
            return null;
        }
    }

    /**
     * Allows keys created on the TEE path to be backed by software, so that tests can exercise key
     * creation on emulators whose keystore is not hardware-backed.
     */
    @CalledByNativeForTesting
    public static void setAllowSoftwareKeysForTesting(boolean allow) {
        if (!BuildConfig.IS_FOR_TEST) {
            throw new IllegalStateException("Software-backed browser bound keys are test-only.");
        }
        sAllowSoftwareKeysForTesting = allow;
        ResettersForTesting.register(() -> sAllowSoftwareKeysForTesting = false);
    }
}
