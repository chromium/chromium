// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapk.lib.client;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;
import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.SCOPE;
import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.START_URL;
import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.WEB_MANIFEST_URL;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.os.Bundle;
import android.os.StrictMode;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.ui.widget.Toast;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.spec.X509EncodedKeySpec;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

/**
 * Checks whether a URL belongs to a WebAPK, and whether a WebAPK is signed by the WebAPK Minting
 * Server.
 */
public class WebApkValidator {
    private static final String TAG = "WebApkValidator";
    private static final String KEY_FACTORY = "EC"; // aka "ECDSA"
    private static final String MAPSLITE_PACKAGE_NAME = "com.google.android.apps.mapslite";
    private static final String MAPSLITE_URL_PREFIX =
            "https://www.google.com/maps"; // Matches scope.
    private static final boolean DEBUG = false;

    private static byte[] sExpectedSignature;
    private static byte[] sCommentSignedPublicKeyBytes;
    private static PublicKey sCommentSignedPublicKey;
    private static boolean sOverrideValidation;

    @IntDef({
        ValidationResult.FAILURE,
        ValidationResult.V1_WEB_APK,
        ValidationResult.MAPS_LITE,
        ValidationResult.COMMENT_SIGNED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ValidationResult {
        int FAILURE = 0;
        int V1_WEB_APK = 1;
        int MAPS_LITE = 2;
        int COMMENT_SIGNED = 3;
    }

    /**
     * Queries the PackageManager to determine whether one or more WebAPKs can handle the URL.
     * Ignores whether the user has selected a default handler for the URL and whether the default
     * handler is a WebAPK.
     *
     * @param context The application context.
     * @param url The url to check.
     * @return Package name for one of the WebAPKs which can handle the URL. If there are several
     *     matching WebAPKs an arbitrary one is returned. Null if there is no matching WebAPK.
     */
    public static @Nullable String queryFirstWebApkPackage(Context context, String url) {
        return findFirstWebApkPackage(context, resolveInfosForUrl(context, url));
    }

    /**
     * Queries the PackageManager to determine whether one or more WebAPKs can handle the URL.
     * Ignores whether the user has selected a default handler for the URL and whether the default
     * handler is a WebAPK.
     *
     * @param context The application context.
     * @param url The url to check.
     * @return ResolveInfo for one of the WebAPKs which can handle the URL. If there are several
     *     matching ResolveInfos an arbitrary one is returned. Null if there is no matching WebAPK.
     */
    public static @Nullable ResolveInfo queryFirstWebApkResolveInfo(Context context, String url) {
        return findFirstWebApkResolveInfo(context, resolveInfosForUrl(context, url));
    }

    /**
     * Queries the PackageManager to determine whether one or more WebAPKs can handle the URL.
     * Ignores whether the user has selected a default handler for the URL and whether the default
     * handler is a WebAPK.
     *
     * @param context The application context.
     * @param url The url to check.
     * @param packageName The optional package name.
     * @return ResolveInfo for one of the WebAPKs which can handle the URL. If there are several
     *     matching ResolveInfos an arbitrary one is returned. Null if there is no matching WebAPK.
     */
    public static @Nullable ResolveInfo queryFirstWebApkResolveInfo(
            Context context, String url, @Nullable String packageName) {
        return findFirstWebApkResolveInfo(
                context, resolveInfosForUrlAndOptionalPackage(context, url, packageName));
    }

    /**
     * Searches {@link infos} and returns the package name of the first {@link ResolveInfo} which
     * corresponds to a WebAPK.
     *
     * @param context The context to use to check whether WebAPK is valid.
     * @param infos The {@link ResolveInfo}s to search.
     * @return WebAPK package name of the match. Null if there are no matches.
     */
    public static @Nullable String findFirstWebApkPackage(
            Context context, List<ResolveInfo> infos) {
        ResolveInfo resolveInfo = findFirstWebApkResolveInfo(context, infos);
        if (resolveInfo != null) {
            return resolveInfo.activityInfo.packageName;
        }
        return null;
    }

    private static void showDeprecationWarning(
            Context context, String appName, @StringRes int resId) {
        assert ThreadUtils.runningOnUiThread();
        String text = context.getResources().getString(resId, appName);
        Toast toast = Toast.makeText(context, text, Toast.LENGTH_SHORT);
        toast.show();
    }

    private static Bundle extractWebApkMetaData(Context context, String webApkPackageName) {
        PackageManager packageManager = context.getPackageManager();
        try {
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(
                            webApkPackageName, PackageManager.GET_META_DATA);
            return appInfo.metaData;
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }

    /**
     * Whether the given package corresponds to a WebAPK that can handle the URL. If the
     * corresponding WebAPK is valid but out of date, show a deprecation warning.
     *
     * @param context The application context.
     * @param webApkPackage The package to consider.
     * @param url The URL the package must be able to handle.
     * @return Whether the URL can be handled by that package.
     */
    public static boolean canWebApkHandleUrl(
            Context context, String webApkPackage, String url, int minShellVersion) {
        List<ResolveInfo> infos = resolveInfosForUrlAndOptionalPackage(context, url, webApkPackage);
        for (ResolveInfo info : infos) {
            if (info.activityInfo != null) {
                @ValidationResult
                int result = isValidWebApkInternal(context, info.activityInfo.packageName);
                switch (result) {
                    case ValidationResult.FAILURE:
                        continue;
                    case ValidationResult.MAPS_LITE:
                        String name = info.loadLabel(context.getPackageManager()).toString();
                        showDeprecationWarning(
                                context, name, R.string.webapk_mapsgo_deprecation_warning);
                        return false;
                    case ValidationResult.V1_WEB_APK:
                        int shellApkVersion =
                                IntentUtils.safeGetInt(
                                        extractWebApkMetaData(context, webApkPackage),
                                        WebApkMetaDataKeys.SHELL_APK_VERSION,
                                        0);
                        if (0 < shellApkVersion && shellApkVersion < minShellVersion) {
                            showDeprecationWarning(
                                    context,
                                    info.loadLabel(context.getPackageManager()).toString(),
                                    R.string.webapk_deprecation_warning);
                            return false;
                        }
                        return true;
                    case ValidationResult.COMMENT_SIGNED:
                        return true;
                    default:
                        assert false;
                }
            }
        }
        return false;
    }

    /**
     * Fetches a list of {@link ResolveInfo}s from the PackageManager that can handle the URL.
     *
     * @param context The application context.
     * @param url The URL to check.
     * @return The list of {@link ResolveInfo}s found that can handle the URL.
     */
    public static List<ResolveInfo> resolveInfosForUrl(Context context, String url) {
        return resolveInfosForUrlAndOptionalPackage(context, url, null);
    }

    /**
     * Fetches the list of {@link ResolveInfo}s from the PackageManager that can handle the URL.
     *
     * @param context The application context.
     * @param url The URL to check.
     * @param applicationPackage The optional package name to set for intent resolution.
     * @return The list of {@link ResolveInfo}s found that can handle the URL.
     */
    private static List<ResolveInfo> resolveInfosForUrlAndOptionalPackage(
            Context context, String url, @Nullable String applicationPackage) {
        Intent intent = createWebApkIntentForUrlAndOptionalPackage(url, applicationPackage);
        if (intent == null) return new LinkedList<>();

        // StrictMode is relaxed due to https://crbug.com/843092.
        StrictMode.ThreadPolicy policy = StrictMode.allowThreadDiskReads();
        try {
            return context.getPackageManager()
                    .queryIntentActivities(
                            intent,
                            PackageManager.GET_RESOLVED_FILTER | PackageManager.GET_META_DATA);
        } catch (Exception e) {
            // We used to catch only java.util.MissingResourceException, but we need to catch
            // more exceptions to handle "Package manager has died" exception.
            // http://crbug.com/794363
            return new LinkedList<>();
        } finally {
            StrictMode.setThreadPolicy(policy);
        }
    }

    /**
     * Searches {@link infos} and returns the first {@link ResolveInfo} which corresponds to a
     * WebAPK.
     *
     * @param context The context to use to check whether WebAPK is valid.
     * @param infos The {@link ResolveInfo}s to search.
     * @return The matching {@link ResolveInfo}. Null if there are no matches.
     */
    private static @Nullable ResolveInfo findFirstWebApkResolveInfo(
            Context context, List<ResolveInfo> infos) {
        for (ResolveInfo info : infos) {
            if (info.activityInfo != null
                    && isValidWebApk(context, info.activityInfo.packageName)) {
                return info;
            }
        }
        return null;
    }

    /**
     * Returns whether the provided WebAPK is installed and passes signature checks.
     *
     * @param context A context
     * @param webappPackageName The package name to check
     * @return true iff the WebAPK is installed and passes security checks
     */
    public static boolean isValidWebApk(Context context, String webappPackageName) {
        return isValidWebApkInternal(context, webappPackageName) != ValidationResult.FAILURE;
    }

    /**
     * Returns whether the provided WebAPK is installed and is valid V1 WebAPK. This is similar to
     * |isValidWebApk| but only checks V1 WebApks, does not checks MapsLite and comment signed
     * WebAPK.
     *
     * @param context A context
     * @param webappPackageName The package name to check
     * @return true iff the WebAPK is installed and passes security checks for V1 WebAPK.
     */
    @SuppressLint("PackageManagerGetSignatures")
    public static boolean isValidV1WebApk(Context context, String webappPackageName) {
        return isValidWebApkInternal(context, webappPackageName) == ValidationResult.V1_WEB_APK;
    }

    @SuppressLint("PackageManagerGetSignatures")
    private static @ValidationResult int isValidWebApkInternal(
            Context context, String webappPackageName) {
        if (sExpectedSignature == null || sCommentSignedPublicKeyBytes == null) {
            Log.wtf(
                    TAG,
                    "WebApk validation failure - expected signature not set - "
                            + "missing call to WebApkValidator.init");
            return ValidationResult.FAILURE;
        }
        PackageInfo packageInfo;
        try {
            packageInfo =
                    context.getPackageManager()
                            .getPackageInfo(
                                    webappPackageName,
                                    PackageManager.GET_SIGNATURES | PackageManager.GET_META_DATA);
        } catch (Exception e) {
            if (DEBUG) {
                e.printStackTrace();
                Log.d(TAG, "WebApk not found");
            }
            return ValidationResult.FAILURE;
        }
        if (isNotWebApkQuick(packageInfo)) {
            return ValidationResult.FAILURE;
        }
        if (sOverrideValidation) {
            if (DEBUG) {
                Log.d(TAG, "WebApk validation is disabled for testing.");
            }
            // Always return V1_WEB_APK in this case, because we only care if it's V1 WebAPK.
            return ValidationResult.V1_WEB_APK;
        }
        if (verifyV1WebApk(packageInfo, webappPackageName)) {
            return ValidationResult.V1_WEB_APK;
        }
        if (verifyMapsLite(packageInfo, webappPackageName)) {
            if (DEBUG) {
                Log.d(TAG, "Matches Maps Lite");
            }

            return ValidationResult.MAPS_LITE;
        }
        if (verifyCommentSignedWebApk(packageInfo)) {
            return ValidationResult.COMMENT_SIGNED;
        }
        return ValidationResult.FAILURE;
    }

    /**
     * @param url A Url that might launch a WebApk.
     * @param applicationPackage The package of the WebApk to restrict the launch to.
     * @return An intent that could launch a WebApk for the provided URL (and package), if such a
     *     WebApk exists. If package isn't specified, the intent may create a disambiguation dialog
     *     when started.
     */
    public static Intent createWebApkIntentForUrlAndOptionalPackage(
            String url, @Nullable String applicationPackage) {
        Intent intent;
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (Exception e) {
            return null;
        }

        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        if (applicationPackage != null) {
            intent.setPackage(applicationPackage);
        } else {
            intent.setComponent(null);
        }
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }
        return intent;
    }

    /** Determine quickly whether this is definitely not a WebAPK */
    private static boolean isNotWebApkQuick(PackageInfo packageInfo) {
        if (packageInfo.applicationInfo == null || packageInfo.applicationInfo.metaData == null) {
            Log.e(TAG, "no application info, or metaData retrieved.");
            return true;
        }
        // Having the startURL in AndroidManifest.xml is a strong signal.
        String startUrl = packageInfo.applicationInfo.metaData.getString(START_URL);
        return TextUtils.isEmpty(startUrl);
    }

    private static boolean verifyV1WebApk(PackageInfo packageInfo, String webappPackageName) {
        if (packageInfo.signatures == null
                || packageInfo.signatures.length != 2
                || !webappPackageName.startsWith(WEBAPK_PACKAGE_PREFIX)) {
            return false;
        }
        for (Signature signature : packageInfo.signatures) {
            if (Arrays.equals(sExpectedSignature, signature.toByteArray())) {
                if (DEBUG) {
                    Log.d(TAG, "WebApk valid - signature match!");
                }
                return true;
            }
        }
        return false;
    }

    private static boolean verifyMapsLite(PackageInfo packageInfo, String webappPackageName) {
        if (!webappPackageName.equals(MAPSLITE_PACKAGE_NAME)) {
            return false;
        }
        String startUrl = packageInfo.applicationInfo.metaData.getString(START_URL);
        if (startUrl == null || !startUrl.startsWith(MAPSLITE_URL_PREFIX)) {
            if (DEBUG) {
                Log.d(TAG, "mapslite invalid startUrl prefix");
            }
            return false;
        }
        String scope = packageInfo.applicationInfo.metaData.getString(SCOPE);
        if (scope == null || !scope.equals(MAPSLITE_URL_PREFIX)) {
            if (DEBUG) {
                Log.d(TAG, "mapslite invalid scope prefix");
            }
            return false;
        }
        return true;
    }

    /** Verify that the comment signed webapk matches the public key. */
    private static boolean verifyCommentSignedWebApk(PackageInfo packageInfo) {
        PublicKey commentSignedPublicKey;
        try {
            commentSignedPublicKey = getCommentSignedPublicKey();
        } catch (Exception e) {
            Log.e(TAG, "WebApk failed to get Public Key", e);
            return false;
        }
        if (commentSignedPublicKey == null) {
            Log.e(TAG, "WebApk validation failure - unable to decode public key");
            return false;
        }
        if (packageInfo.applicationInfo == null || packageInfo.applicationInfo.sourceDir == null) {
            Log.e(TAG, "WebApk validation failure - missing applicationInfo sourcedir");
            return false;
        }

        String packageFilename = packageInfo.applicationInfo.sourceDir;
        RandomAccessFile file = null;
        FileChannel inChannel = null;
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();

        try {
            file = new RandomAccessFile(packageFilename, "r");
            inChannel = file.getChannel();

            MappedByteBuffer buf =
                    inChannel.map(FileChannel.MapMode.READ_ONLY, 0, inChannel.size());
            buf.load();

            WebApkVerifySignature v = new WebApkVerifySignature(buf);
            @WebApkVerifySignature.Error int result = v.read();
            if (result != WebApkVerifySignature.Error.OK) {
                Log.e(TAG, String.format("Failure reading %s: %s", packageFilename, result));
                return false;
            }
            result = v.verifySignature(commentSignedPublicKey);

            // TODO(scottkirkwood): remove this log once well tested.
            if (DEBUG) {
                Log.d(TAG, "File " + packageFilename + ": " + result);
            }
            return result == WebApkVerifySignature.Error.OK;
        } catch (Exception e) {
            Log.e(TAG, "WebApk file error for file " + packageFilename, e);
            return false;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
            if (inChannel != null) {
                try {
                    inChannel.close();
                } catch (IOException e) {
                }
            }
            if (file != null) {
                try {
                    file.close();
                } catch (IOException e) {
                }
            }
        }
    }

    /**
     * Determines if a bound WebAPK generated from |manifestUrl| is installed on the device.
     *
     * @param context The context to use to check whether WebAPK is valid.
     * @param manifestUrl The URL of the manifest that was used to generate the WebAPK.
     * @return The WebAPK's package name if installed, or null otherwise.
     */
    @SuppressWarnings("QueryPermissionsNeeded")
    public static @Nullable String queryBoundWebApkForManifestUrl(
            Context context, String manifestUrl) {
        assert manifestUrl != null;

        List<PackageInfo> packages =
                context.getPackageManager()
                        .getInstalledPackages(
                                PackageManager.GET_SIGNATURES | PackageManager.GET_META_DATA);

        // Filter out unbound & invalid WebAPKs.
        for (int i = 0; i < packages.size(); i++) {
            PackageInfo info = packages.get(i);

            if (!verifyV1WebApk(info, info.packageName)) {
                continue;
            }

            // |info| belongs to a valid, bound, WebAPK. Check that the metadata contains
            // |manifestUrl|.
            String packageManifestUrl = info.applicationInfo.metaData.getString(WEB_MANIFEST_URL);
            if (!TextUtils.equals(packageManifestUrl, manifestUrl)) {
                continue;
            }

            return info.packageName;
        }

        return null;
    }

    /**
     * Initializes the WebApkValidator.
     *
     * @param expectedSignature V1 WebAPK RSA signature.
     * @param v2PublicKeyBytes New comment signed public key bytes as x509 encoded public key.
     */
    public static void init(byte[] expectedSignature, byte[] v2PublicKeyBytes) {
        if (sExpectedSignature == null) {
            sExpectedSignature = expectedSignature;
        }
        if (sCommentSignedPublicKeyBytes == null) {
            sCommentSignedPublicKeyBytes = v2PublicKeyBytes;
        }
    }

    /**
     * Sets whether validation performed by this class should be disabled. This is meant only for
     * development with unsigned WebApks and should never be enabled in a real build.
     */
    public static void setDisableValidationForTesting(boolean disable) {
        var oldValue = sOverrideValidation;
        sOverrideValidation = disable;
        ResettersForTesting.register(() -> sOverrideValidation = oldValue);
    }

    /**
     * Sets whether validation performed by this class should be disabled. This is meant only for
     * development with unsigned WebApks and should never be enabled in a real build.
     */
    public static void setDisableValidation(boolean disable) {
        sOverrideValidation = disable;
    }

    /**
     * Lazy evaluate the creation of the Public Key as the KeyFactories may not yet be initialized.
     *
     * @return The decoded PublicKey or null
     */
    private static PublicKey getCommentSignedPublicKey() throws Exception {
        if (sCommentSignedPublicKey == null) {
            sCommentSignedPublicKey =
                    KeyFactory.getInstance(KEY_FACTORY)
                            .generatePublic(new X509EncodedKeySpec(sCommentSignedPublicKeyBytes));
        }
        return sCommentSignedPublicKey;
    }
}
