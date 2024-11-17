// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_relationship_verification;

import android.net.Uri;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Use to check that an app has a Digital Asset Link relationship with the given origin.
 *
 * Multiple instances of this object share a static cache, and as such the static
 * {@link #wasPreviouslyVerified} can be used to check whether any verification has been carried
 * out.
 *
 * One instance of this object should be created per package, but {@link #start} may be called
 * multiple times to verify different origins. This object has a native counterpart that will be
 * kept alive as it is serving requests, but destroyed once all requests are finished.
 *
 */
public abstract class OriginVerifier {
    private static final String TAG = "OriginVerifier";

    public static final String USE_AS_ORIGIN = "delegate_permission/common.use_as_origin";
    public static final String HANDLE_ALL_URLS = "delegate_permission/common.handle_all_urls";

    public final String mPackageName;
    public final List<String> mSignatureFingerprints;
    public final String mRelation;
    public final Map<Origin, Set<OriginVerificationListener>> mListeners = new HashMap<>();

    private long mNativeOriginVerifier;
    private long mVerificationStartTime;
    private final VerificationResultStore mVerificationResultStore;

    @Nullable public WebContents mWebContents;

    public static enum VerifierResult {
        ONLINE_SUCCESS,
        ONLINE_FAILURE,
        OFFLINE_SUCCESS,
        OFFLINE_FAILURE,
        HTTPS_FAILURE,
        REQUEST_FAILURE,
    }

    /** Small helper class to post a result of origin verification. */
    public class VerifiedCallback implements Runnable {
        private final Origin mOrigin;
        private final boolean mResult;
        private final Boolean mOnline;

        public VerifiedCallback(Origin origin, boolean result, Boolean online) {
            mOrigin = origin;
            mResult = result;
            mOnline = online;
        }

        @Override
        public void run() {
            originVerified(mOrigin, mResult, mOnline);
        }
    }

    public static Uri getPostMessageUriFromVerifiedOrigin(
            String packageName, Origin verifiedOrigin) {
        return Uri.parse(
                IntentUtils.ANDROID_APP_REFERRER_SCHEME
                        + "://"
                        + verifiedOrigin.uri().getHost()
                        + "/"
                        + packageName);
    }

    /** Callback interface for getting verification results. */
    public interface OriginVerificationListener {
        /**
         * To be posted on the handler thread after the verification finishes.
         * @param packageName The package name for the origin verification query for this result.
         * @param origin The origin that was declared on the query for this result.
         * @param verified Whether the given origin was verified to correspond to the given package.
         * @param online Whether the device could connect to the internet to perform verification.
         *               Will be {@code null} if internet was not required for check (eg
         *               verification had already been attempted this Chrome lifetime and the
         *               result was cached or the origin was not https).
         */
        void onOriginVerified(String packageName, Origin origin, boolean verified, Boolean online);
    }

    /**
     * Main constructor. Use {@link OriginVerifier#start}
     *
     * @param packageName The package for the Android application for verification.
     * @param relation Digital Asset Links relation to use during verification, one of
     *     "delegate_permission/common.use_as_origin", "delegate_permission/common.handle_all_urls".
     * @param webContents The web contents of the tab used for reporting errors to DevTools. Can be
     *     null if unavailable.
     * @param browserContextHandle handle to retrieve the browser context for creating the url
     *     loader factory. If null, initialize the native side before calling {@code start}
     * @param verificationResultStore The {@link VerificationResultStore} for persisting results.
     */
    public OriginVerifier(
            String packageName,
            String relation,
            @Nullable WebContents webContents,
            @Nullable BrowserContextHandle browserContextHandle,
            VerificationResultStore verificationResultStore) {
        mPackageName = packageName;

        mSignatureFingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(packageName);

        mRelation = relation;
        mWebContents = webContents;

        mVerificationResultStore = verificationResultStore;

        if (browserContextHandle != null) {
            initNativeOriginVerifier(browserContextHandle);
        }
    }

    /**
     * Verify the claimed origin for the cached package name asynchronously. This will end up
     * making a network request for non-cached origins with a URLFetcher using the last used
     * profile as context.
     * @param listener The listener who will get the verification result.
     * @param origin The postMessage origin the application is claiming to have. Can't be null.
     */
    public void start(@NonNull OriginVerificationListener listener, @NonNull Origin origin) {
        ThreadUtils.assertOnUiThread();
        if (mListeners.containsKey(origin)) {
            // We already have an ongoing verification for that origin, just add the listener.
            mListeners.get(origin).add(listener);
            return;
        } else {
            mListeners.put(origin, new HashSet<>());
            mListeners.get(origin).add(listener);
        }
        validate(origin);
    }

    /**
     * Performs the DAL-validation, do not call directly, call #start to register listeners for
     * receiving the results of the validation.
     */
    public void validate(@NonNull Origin origin) {
        assert mNativeOriginVerifier != 0
                : "Either provide a browserContextHandle to "
                        + "OriginVerifier#ctor or call initNativeOriginVerifier.";

        String scheme = origin.uri().getScheme();
        String host = origin.uri().getHost();
        if (TextUtils.isEmpty(scheme)
                || (UrlConstants.HTTP_SCHEME.equals(scheme.toLowerCase(Locale.US))
                        && !UrlConstants.LOCALHOST.equals(host.toLowerCase(Locale.US)))) {
            Log.i(TAG, "Verification failed for %s as not https or localhost.", origin);
            recordResultMetrics(VerifierResult.HTTPS_FAILURE);
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT, new VerifiedCallback(origin, false, null));
            return;
        }

        if (mVerificationResultStore.shouldOverride(mPackageName, origin, mRelation)) {
            Log.i(TAG, "Verification succeeded for %s, it was overridden.", origin);
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, new VerifiedCallback(origin, true, null));
            return;
        }

        if (isAllowlisted(mPackageName, origin, mRelation)) {
            Log.i(
                    TAG,
                    "Verification succeeded for %s, %s, it was allowlisted.",
                    mPackageName,
                    origin);
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, new VerifiedCallback(origin, true, null));
            return;
        }

        if (mWebContents != null && mWebContents.isDestroyed()) mWebContents = null;

        mVerificationStartTime = SystemClock.uptimeMillis();
        String[] fingerprints =
                mSignatureFingerprints == null
                        ? null
                        : mSignatureFingerprints.toArray(new String[0]);

        boolean requestSent =
                OriginVerifierJni.get()
                        .verifyOrigin(
                                mNativeOriginVerifier,
                                OriginVerifier.this,
                                mPackageName,
                                fingerprints,
                                origin.toString(),
                                mRelation,
                                mWebContents);
        if (!requestSent) {
            recordResultMetrics(VerifierResult.REQUEST_FAILURE);
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT, new VerifiedCallback(origin, false, false));
        }
    }

    /** Cleanup native dependencies on this object. */
    public void cleanUp() {
        // Only destroy native once we have no other pending verifications.
        if (!mListeners.isEmpty()) return;
        if (mNativeOriginVerifier == 0) return;
        OriginVerifierJni.get().destroy(mNativeOriginVerifier, OriginVerifier.this);
        mNativeOriginVerifier = 0;
    }

    /** Called asynchronously by OriginVerifierJni.get().verifyOrigin. */
    @VisibleForTesting
    @CalledByNative
    public void onOriginVerificationResult(String originAsString, int result) {
        Origin origin = Origin.createOrThrow(originAsString);
        switch (result) {
            case RelationshipCheckResult.SUCCESS:
                recordResultMetrics(VerifierResult.ONLINE_SUCCESS);
                originVerified(origin, true, true);
                break;
            case RelationshipCheckResult.FAILURE:
                recordResultMetrics(VerifierResult.ONLINE_FAILURE);
                originVerified(origin, false, true);
                break;
            case RelationshipCheckResult.NO_CONNECTION:
                Log.i(TAG, "Device is offline, checking saved verification result.");
                boolean storedResult = checkForSavedResult(origin);
                recordResultMetrics(
                        storedResult
                                ? VerifierResult.OFFLINE_SUCCESS
                                : VerifierResult.OFFLINE_FAILURE);
                originVerified(origin, storedResult, false);
                break;
            default:
                assert false;
        }
    }

    /** Deal with the result of an Origin check. Will be called on UI Thread. */
    private void originVerified(Origin origin, boolean originVerified, Boolean online) {
        if (originVerified) {
            Log.d(TAG, "Adding: %s for %s", mPackageName, origin);
            mVerificationResultStore.addRelationship(
                    new Relationship(mPackageName, mSignatureFingerprints, origin, mRelation));
        } else {
            Log.d(
                    TAG,
                    "Digital Asset Link verification failed for package %s with "
                            + "fingerprint %s.",
                    mPackageName,
                    mSignatureFingerprints);
        }

        // We save the result even if there is a failure as a way of overwriting a previously
        // successfully verified result that fails on a subsequent check.
        saveVerificationResult(origin, originVerified);

        if (mListeners.containsKey(origin)) {
            Set<OriginVerificationListener> listeners = mListeners.get(origin);
            for (OriginVerificationListener listener : listeners) {
                listener.onOriginVerified(mPackageName, origin, originVerified, online);
            }
            mListeners.remove(origin);
        }

        if (online != null) {
            long duration = SystemClock.uptimeMillis() - mVerificationStartTime;
            recordVerificationTimeMetrics(duration, online);
        }

        cleanUp();
    }

    /** Saves the result of a verification to Preferences so we can reuse it when offline. */
    private void saveVerificationResult(Origin origin, boolean originVerified) {
        Relationship relationship =
                new Relationship(mPackageName, mSignatureFingerprints, origin, mRelation);
        if (originVerified) {
            mVerificationResultStore.addRelationship(relationship);
        } else {
            mVerificationResultStore.removeRelationship(relationship);
        }
    }

    /** Checks for a previously saved verification result. */
    public boolean checkForSavedResult(Origin origin) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return mVerificationResultStore.isRelationshipSaved(
                    new Relationship(mPackageName, mSignatureFingerprints, origin, mRelation));
        }
    }

    public boolean checkForSavedResult(String url) {
        return checkForSavedResult(Origin.create(url));
    }

    /** Initialization of the native OriginVerifier. */
    public void initNativeOriginVerifier(BrowserContextHandle browserContextHandle) {
        mNativeOriginVerifier =
                OriginVerifierJni.get().init(OriginVerifier.this, browserContextHandle);
    }

    public boolean isNativeOriginVerifierInitialized() {
        return mNativeOriginVerifier != 0;
    }

    public void setNativeOriginVerifier(long nativeOriginVerifier) {
        mNativeOriginVerifier = nativeOriginVerifier;
    }

    @VisibleForTesting
    public int getNumListeners(Origin origin) {
        if (mListeners.containsKey(origin)) {
            return mListeners.get(origin).size();
        }
        return 0;
    }

    /** Implement exceptions that can bypass DAL-validation. */
    public abstract boolean isAllowlisted(String packageName, Origin origin, String relation);

    /**
     * Checks whether the origin was verified for that origin with a call to {@link #start}.
     * Abstract as this requires an instance of VerificationResultStore.
     */
    public abstract boolean wasPreviouslyVerified(Origin origin);

    /** Implement for logging of VerifierResult for different embedders. */
    public abstract void recordResultMetrics(VerifierResult result);

    /** Implement for logging of VerificationTimeMetrics for different embedders. */
    public abstract void recordVerificationTimeMetrics(long duration, boolean online);

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        long init(OriginVerifier caller, BrowserContextHandle browserContextHandle);

        boolean verifyOrigin(
                long nativeOriginVerifier,
                OriginVerifier caller,
                String packageName,
                String[] signatureFingerprint,
                String origin,
                String relationship,
                @Nullable WebContents webContents);

        void destroy(long nativeOriginVerifier, OriginVerifier caller);
    }
}
