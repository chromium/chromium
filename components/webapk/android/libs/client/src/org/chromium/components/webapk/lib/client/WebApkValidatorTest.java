// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapk.lib.client;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.SCOPE;
import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.SHELL_APK_VERSION;
import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.START_URL;
import static org.chromium.components.webapk.lib.common.WebApkMetaDataKeys.WEB_MANIFEST_URL;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.os.Bundle;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.testing.local.TestDir;
import org.chromium.ui.widget.ToastManager;

import java.net.URISyntaxException;

/** Unit tests for {@link org.chromium.webapk.lib.client.WebApkValidator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkValidatorTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.foo";
    private static final String INVALID_WEBAPK_PACKAGE_NAME = "invalid.org.chromium.webapk.foo";
    private static final String URL_OF_WEBAPK = "https://www.foo.com";
    private static final String URL_WITHOUT_WEBAPK = "https://www.other.com";
    private static final String TEST_DATA_DIR = "webapks/";
    private static final String TEST_STARTURL = "https://non-empty.com/starturl";
    private static final String MAPSLITE_PACKAGE_NAME = "com.google.android.apps.mapslite";
    private static final String MAPSLITE_EXAMPLE_STARTURL = "https://www.google.com/maps";
    private static final String MANIFEST_URL = "https://www.foo.com/manifest.json";
    private static final int SHELL_VERSION = 100;

    private static final byte[] EXPECTED_SIGNATURE =
            new byte[] {
                48, -126, 3, -121, 48, -126, 2, 111, -96, 3, 2, 1, 2, 2, 4, 20, -104, -66, -126, 48,
                13, 6, 9, 42, -122, 72, -122, -9, 13, 1, 1, 11, 5, 0, 48, 116, 49, 11, 48, 9, 6, 3,
                85, 4, 6, 19, 2, 67, 65, 49, 16, 48, 14, 6, 3, 85, 4, 8, 19, 7, 79, 110, 116, 97,
                114, 105, 111, 49, 17, 48, 15, 6, 3, 85, 4, 7, 19, 8, 87, 97, 116, 101, 114, 108,
                111, 111, 49, 17, 48, 15, 6, 3, 85, 4, 10, 19, 8, 67, 104, 114, 111, 109, 105, 117,
                109, 49, 17, 48
            };

    private static final byte[] SIGNATURE_1 =
            new byte[] {
                13, 52, 51, 48, 51, 48, 51, 49, 53, 49, 54, 52, 52, 90, 48, 116, 49, 11, 48, 9, 6,
                3, 85, 4, 6, 19, 2, 67, 65, 49, 16, 48, 14, 6, 3, 85, 4, 8, 19, 7, 79, 110, 116, 97,
                114
            };

    private static final byte[] SIGNATURE_2 =
            new byte[] {
                49, 17, 48, 15, 6, 3, 85, 4, 10, 19, 8, 67, 104, 114, 111, 109, 105, 117, 109, 49,
                17, 48, 15, 6, 3, 85, 4, 11, 19, 8, 67, 104, 114, 111, 109, 105, 117, 109, 49, 26,
                48, 24
            };

    // This is the public key used for the test files (chrome/test/data/webapks/public.der)
    private static final byte[] PUBLIC_KEY =
            new byte[] {
                48, 89, 48, 19, 6, 7, 42, -122, 72, -50, 61, 2, 1, 6, 8, 42, -122, 72, -50, 61, 3,
                1, 7, 3, 66, 0, 4, -67, 14, 37, -20, 103, 121, 124, -60, -21, 83, -114, -120, -87,
                -38, 26, 78, 82, 55, 44, -23, -2, 104, 115, 82, -55, -104, 105, -19, -48, 89, -65,
                12, -31, 16, -35, 4, -121, -70, -89, 23, 56, 115, 112, 78, -65, 114, -103, 120, -88,
                -112, -102, -61, 72, -16, 74, 53, 50, 49, -56, -48, -90, 5, -116, 78
            };

    private ShadowPackageManager mPackageManager;

    @Before
    public void setUp() {
        mPackageManager = Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        WebApkValidator.init(EXPECTED_SIGNATURE, PUBLIC_KEY);
    }

    @After
    public void tearDown() {
        ToastManager.resetForTesting();
        ShadowToast.reset();
    }

    /**
     * Tests {@link WebApkValidator.queryFirstWebApkPackage()} returns a WebAPK's package name if
     * the WebAPK can handle the given URL and the WebAPK is valid.
     */
    @Test
    public void testQueryWebApkPackageReturnsPackageIfTheURLCanBeHandled() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME,
                            new Signature(EXPECTED_SIGNATURE),
                            TEST_STARTURL,
                            null));

            assertEquals(
                    WEBAPK_PACKAGE_NAME,
                    WebApkValidator.queryFirstWebApkPackage(
                            RuntimeEnvironment.application, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /**
     * Tests {@link WebApkValidator.queryFirstWebApkPackage()} returns null for a non-browsable
     * Intent.
     */
    @Test
    public void testQueryWebApkPackageReturnsNullForNonBrowsableIntent() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME,
                            new Signature(EXPECTED_SIGNATURE),
                            TEST_STARTURL,
                            null));

            assertNull(
                    WebApkValidator.queryFirstWebApkPackage(
                            RuntimeEnvironment.application, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /**
     * Tests {@link WebApkValidator.queryFirstWebApkPackage()} returns null if no WebAPK handles the
     * given URL.
     */
    @Test
    public void testQueryWebApkPackageReturnsNullWhenNoWebApkHandlesTheURL() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME,
                            new Signature(EXPECTED_SIGNATURE),
                            TEST_STARTURL,
                            null));

            assertNull(
                    WebApkValidator.queryFirstWebApkPackage(
                            RuntimeEnvironment.application, URL_WITHOUT_WEBAPK));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns true if the WebAPK can handle the
     * given URL and the WebAPK is valid.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsTrueIfTheURLCanBeHandled() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME,
                            new Signature(EXPECTED_SIGNATURE),
                            TEST_STARTURL,
                            null));

            assertTrue(
                    WebApkValidator.canWebApkHandleUrl(
                            RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_OF_WEBAPK, 0));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns false if the given APK package
     * name is not signed with the WebAPK signature.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsFalseIfWebApkIsNotValid() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME, new Signature(SIGNATURE_1), TEST_STARTURL, null));

            assertFalse(
                    WebApkValidator.canWebApkHandleUrl(
                            RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_OF_WEBAPK, 0));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /** Tests {@link WebApkValidator.canWebApkHandleUrl()} returns false for a non-browsable WebAPK. */
    @Test
    public void testCanWebApkHandleUrlReturnsFalseForNonBrowsableIntent() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME,
                            new Signature(EXPECTED_SIGNATURE),
                            TEST_STARTURL,
                            null));

            assertFalse(
                    WebApkValidator.canWebApkHandleUrl(
                            RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_OF_WEBAPK, 0));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns false if the specific WebAPK does
     * not handle the given URL.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsFalseWhenNoWebApkHandlesTheURL() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            WEBAPK_PACKAGE_NAME,
                            new Signature(EXPECTED_SIGNATURE),
                            TEST_STARTURL,
                            null));

            assertFalse(
                    WebApkValidator.canWebApkHandleUrl(
                            RuntimeEnvironment.application,
                            WEBAPK_PACKAGE_NAME,
                            URL_WITHOUT_WEBAPK,
                            0));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns true if a package name corresponds to a
     * WebAPK and the WebAPK is valid.
     */
    @Test
    public void testIsValidWebApkReturnsTrueForValidWebApk() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        TEST_STARTURL,
                        null));

        assertTrue(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false if the package name is not valid
     * for WebApks (and isn't comment-signed).
     */
    @Test
    public void testIsValidWebApkFalseForInvalidPackageName() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        INVALID_WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        TEST_STARTURL,
                        null));

        assertFalse(
                WebApkValidator.isValidWebApk(
                        RuntimeEnvironment.application, INVALID_WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns true if the package name is maps lite and
     * the start url matches the correct prefix.
     */
    @Test
    public void testIsValidWebApkForMapsLite() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        MAPSLITE_PACKAGE_NAME,
                        new Signature(SIGNATURE_1),
                        MAPSLITE_EXAMPLE_STARTURL,
                        null));
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        MAPSLITE_PACKAGE_NAME + ".other",
                        new Signature(SIGNATURE_1),
                        MAPSLITE_EXAMPLE_STARTURL,
                        null));

        assertTrue(
                WebApkValidator.isValidWebApk(
                        RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME));
        assertFalse(
                WebApkValidator.isValidWebApk(
                        RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME + ".other"));
        assertFalse(
                WebApkValidator.isValidWebApk(
                        RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME + ".notfound"));
    }

    /** Tests {@link WebApkValidator.canWebApkHandleUrl} returns false and shows a toast. */
    @Test
    public void testMapsLiteWebApkShowsWarning() {
        // Invalid MapsLite WebAPK is not verified and does not show toast.
        addWebApkResolveInfoWithPackageName(
                MAPSLITE_EXAMPLE_STARTURL, MAPSLITE_PACKAGE_NAME + ".other", SIGNATURE_1);
        assertFalse(
                WebApkValidator.canWebApkHandleUrl(
                        RuntimeEnvironment.application,
                        MAPSLITE_PACKAGE_NAME + ".other",
                        MAPSLITE_EXAMPLE_STARTURL,
                        0));
        assertNull(ShadowToast.getLatestToast());

        // Valid MapsLite WebAPK returns false as "not handled" and shows a toast.
        addWebApkResolveInfoWithPackageName(
                MAPSLITE_EXAMPLE_STARTURL, MAPSLITE_PACKAGE_NAME, SIGNATURE_1);
        assertFalse(
                WebApkValidator.canWebApkHandleUrl(
                        RuntimeEnvironment.application,
                        MAPSLITE_PACKAGE_NAME,
                        MAPSLITE_EXAMPLE_STARTURL,
                        0));
        assertNotNull(ShadowToast.getLatestToast());
        // assertTextFromLatestToast(R.string.copied);
        TextView textView = (TextView) ShadowToast.getLatestToast().getView();
        String actualText = textView == null ? "" : textView.getText().toString();
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.webapk_mapsgo_deprecation_warning, ""),
                actualText);
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl} returns false and shows a toast when the
     * shell version is out-of-date (older than the min_version).
     */
    @Test
    public void testOldShellWebApkShowsWarning() {
        addWebApkResolveInfoWithPackageName(URL_OF_WEBAPK, WEBAPK_PACKAGE_NAME, EXPECTED_SIGNATURE);

        // Current Shell Version larger than min_version, can handle URL.
        assertTrue(
                WebApkValidator.canWebApkHandleUrl(
                        RuntimeEnvironment.application,
                        WEBAPK_PACKAGE_NAME,
                        URL_OF_WEBAPK,
                        SHELL_VERSION - 1));
        assertNull(ShadowToast.getLatestToast());

        // Current Shell Version smaller than min_version, returns false as "not handled" and shows
        // a toast.
        assertFalse(
                WebApkValidator.canWebApkHandleUrl(
                        RuntimeEnvironment.application,
                        WEBAPK_PACKAGE_NAME,
                        URL_OF_WEBAPK,
                        SHELL_VERSION + 1));
        assertNotNull(ShadowToast.getLatestToast());
        // assertTextFromLatestToast(R.string.copied);
        TextView textView = (TextView) ShadowToast.getLatestToast().getView();
        String actualText = textView == null ? "" : textView.getText().toString();
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.webapk_deprecation_warning, ""),
                actualText);
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false when the startUrl is not correct.
     */
    @Test
    public void testIsNotValidWebApkForMapsLiteBadStartUrl() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        MAPSLITE_PACKAGE_NAME, new Signature(SIGNATURE_1), TEST_STARTURL, null));
        assertFalse(
                WebApkValidator.isValidWebApk(
                        RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false if a WebAPK has more than 2
     * signatures, even if the second one matches the expected signature.
     */
    @Test
    public void testIsValidWebApkReturnsFalseForMoreThanTwoSignatures() {
        Signature[] signatures =
                new Signature[] {
                    new Signature(SIGNATURE_1),
                    new Signature(EXPECTED_SIGNATURE),
                    new Signature(SIGNATURE_2)
                };
        mPackageManager.addPackage(
                newPackageInfo(WEBAPK_PACKAGE_NAME, signatures, null, TEST_STARTURL, null));

        assertFalse(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false if a WebAPK has multiple signatures
     * but none of the signatures match the expected signature.
     */
    @Test
    public void testIsValidWebApkReturnsFalseForWebApkWithMultipleSignaturesWithoutAnyMatched() {
        Signature[] signatures =
                new Signature[] {new Signature(SIGNATURE_1), new Signature(SIGNATURE_2)};
        mPackageManager.addPackage(
                newPackageInfo(WEBAPK_PACKAGE_NAME, signatures, null, TEST_STARTURL, null));

        assertFalse(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /** Tests {@link WebApkValidator#isValidWebApk()} for valid comment signed webapks. */
    @Test
    public void testIsValidWebApkCommentSigned() {
        String[] filenames = {"example.apk", "java-example.apk", "v2-signed-ok.apk"};
        String packageName = "com.webapk.a9c419502bb98fcb7";
        Signature[] signature = new Signature[] {new Signature(SIGNATURE_1)};

        for (String filename : filenames) {
            mPackageManager.removePackage(packageName);
            mPackageManager.addPackage(
                    newPackageInfo(
                            packageName, signature, testFilePath(filename), TEST_STARTURL, null));
            assertTrue(
                    filename + " did not verify",
                    WebApkValidator.isValidWebApk(RuntimeEnvironment.application, packageName));
        }
    }

    /**
     * Tests {@link WebApkValidator#isValidWebApk()} for failing comment signed webapks. These
     * WebAPKs were modified to fail in specific ways.
     */
    @Test
    public void testIsValidWebApkCommentSignedFailures() {
        String[] filenames = {
            "bad-sig.apk",
            "bad-utf8-fname.apk",
            "empty.apk",
            "extra-len-too-large.apk",
            "fcomment-too-large.apk",
            "no-cd.apk",
            "no-comment.apk",
            "no-eocd.apk",
            "no-lfh.apk",
            "not-an.apk",
            "too-many-metainf.apk",
            "truncated.apk",
            "zeros.apk",
            "zeros-at-end.apk",
            "block-before-first.apk",
            "block-at-end.apk",
            "block-before-eocd.apk",
            "block-before-cd.apk",
            "block-middle.apk",
            "v2-signed-too-large.apk",
        };
        String packageName = "com.webapk.a9c419502bb98fcb7";
        Signature[] signature = new Signature[] {new Signature(SIGNATURE_1)};

        for (String filename : filenames) {
            mPackageManager.removePackage(packageName);
            mPackageManager.addPackage(
                    newPackageInfo(
                            packageName, signature, testFilePath(filename), TEST_STARTURL, null));
            assertFalse(
                    filename,
                    WebApkValidator.isValidWebApk(RuntimeEnvironment.application, packageName));
        }
    }

    /**
     * Tests {@link WebApkValidator.isValidV1WebApk} returns true if a package name corresponds to a
     * WebAPK and the WebAPK is valid.
     */
    @Test
    public void testIsValidV1WebApkReturnsTrueForValidWebApk() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        TEST_STARTURL,
                        null));

        assertTrue(
                WebApkValidator.isValidV1WebApk(
                        RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidV1WebApk} returns false if the package name is not valid
     * for WebApks.
     */
    @Test
    public void testIsValidV1WebApkFalseForInvalidPackageName() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        INVALID_WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        TEST_STARTURL,
                        null));

        assertFalse(
                WebApkValidator.isValidV1WebApk(
                        RuntimeEnvironment.application, INVALID_WEBAPK_PACKAGE_NAME));
    }

    /** Tests {@link WebApkValidator.isValidV1WebApk} returns false if the package name is maps lite. */
    @Test
    public void testIsValidV1WebApkFalseForMapsLite() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        MAPSLITE_PACKAGE_NAME,
                        new Signature(SIGNATURE_1),
                        MAPSLITE_EXAMPLE_STARTURL,
                        null));
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        MAPSLITE_PACKAGE_NAME + ".other",
                        new Signature(SIGNATURE_1),
                        MAPSLITE_EXAMPLE_STARTURL,
                        null));

        assertFalse(
                WebApkValidator.isValidV1WebApk(
                        RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME));
    }

    /** Tests {@link WebApkValidator#queryBoundWebApkForManifestUrl()} for a valid installed entry. */
    @Test
    public void testQueryBoundWebApkForManifestUrl() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        null,
                        MANIFEST_URL));

        assertEquals(
                WEBAPK_PACKAGE_NAME,
                WebApkValidator.queryBoundWebApkForManifestUrl(
                        RuntimeEnvironment.application, MANIFEST_URL));
    }

    /** Tests {@link WebApkValidator#queryBoundWebApkForManifestUrl()} with an invalid package name. */
    @Test
    public void testQueryBoundWebApkForManifestUrlWithInvalidPackageName() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        INVALID_WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        null,
                        MANIFEST_URL));

        assertNull(
                WebApkValidator.queryBoundWebApkForManifestUrl(
                        RuntimeEnvironment.application, MANIFEST_URL));
    }

    /** Tests {@link WebApkValidator#queryBoundWebApkForManifestUrl()} with an invalid signature. */
    @Test
    public void testQueryBoundWebApkForManifestUrlWithInvalidSignature() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME, new Signature(SIGNATURE_1), null, MANIFEST_URL));

        assertNull(
                WebApkValidator.queryBoundWebApkForManifestUrl(
                        RuntimeEnvironment.application, MANIFEST_URL));
    }

    /** Tests {@link WebApkValidator#queryBoundWebApkForManifestUrl()} with an invalid manifest URL. */
    @Test
    public void testQueryBoundWebApkForManifestUrlWithInvalidManifestUrl() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME, new Signature(SIGNATURE_1), null, MANIFEST_URL));

        assertNull(
                WebApkValidator.queryBoundWebApkForManifestUrl(
                        RuntimeEnvironment.application, "https://evil.com/manifest.json"));
    }

    /**
     * Tests when override validation is set, {@link WebApkValidator.isValidWebApk} returns true
     * with invalid signature.
     */
    @Test
    public void testIsValidWebApkWithOverridesSignature() {
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME, new Signature(SIGNATURE_1), TEST_STARTURL, null));

        assertFalse(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));

        WebApkValidator.setDisableValidationForTesting(true);
        assertTrue(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests when override validation is set, {@link WebApkValidator.isValidWebApk} returns false
     * when no START_URL.
     */
    @Test
    public void testIsValidWebApkOverridesReturnsFalseNoStartUrl() {
        PackageInfo webapkPackage =
                newPackageInfoWithBrowserSignature(
                        WEBAPK_PACKAGE_NAME,
                        new Signature(EXPECTED_SIGNATURE),
                        TEST_STARTURL,
                        null);
        webapkPackage.applicationInfo.metaData.remove(START_URL);
        mPackageManager.addPackage(webapkPackage);

        WebApkValidator.setDisableValidationForTesting(true);
        assertFalse(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests when override validation is set, {@link WebApkValidator.isValidWebApk} returns false
     * when package is not installed.
     */
    @Test
    public void testIsValidWebApkOverridesPackageNotFound() {
        WebApkValidator.setDisableValidationForTesting(true);

        assertFalse(
                WebApkValidator.isValidWebApk(
                        RuntimeEnvironment.application, INVALID_WEBAPK_PACKAGE_NAME));
    }

    @Test
    public void testQueryWebApkResolveInfoWithPackageName() {
        addWebApkResolveInfoWithPackageName(URL_OF_WEBAPK, WEBAPK_PACKAGE_NAME, EXPECTED_SIGNATURE);

        ResolveInfo resolveInfo =
                WebApkValidator.queryFirstWebApkResolveInfo(
                        RuntimeEnvironment.application, URL_OF_WEBAPK, WEBAPK_PACKAGE_NAME);

        assertNotNull(resolveInfo);
        assertEquals(resolveInfo.activityInfo.packageName, WEBAPK_PACKAGE_NAME);
    }

    @Test
    public void testQueryWebApkResolveInfoWithInvalidPackageName() {
        addWebApkResolveInfoWithPackageName(
                URL_OF_WEBAPK, INVALID_WEBAPK_PACKAGE_NAME, EXPECTED_SIGNATURE);

        ResolveInfo resolveInfo =
                WebApkValidator.queryFirstWebApkResolveInfo(
                        RuntimeEnvironment.application, URL_OF_WEBAPK, INVALID_WEBAPK_PACKAGE_NAME);

        assertNull(resolveInfo);
    }

    @Test
    public void testQueryWebApkResolveInfoWithInvalidSignature() {
        addWebApkResolveInfoWithPackageName(URL_OF_WEBAPK, WEBAPK_PACKAGE_NAME, SIGNATURE_1);

        ResolveInfo resolveInfo =
                WebApkValidator.queryFirstWebApkResolveInfo(
                        RuntimeEnvironment.application, URL_OF_WEBAPK, WEBAPK_PACKAGE_NAME);

        assertNull(resolveInfo);
    }

    // Get the full test file path.
    private static String testFilePath(String fileName) {
        return TestDir.getTestFilePath(TEST_DATA_DIR + fileName);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private static PackageInfo newPackageInfo(
            String packageName,
            Signature[] signatures,
            String sourceDir,
            String startUrl,
            String manifestUrl) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.signatures = signatures;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.metaData = new Bundle();
        packageInfo.applicationInfo.metaData.putString(START_URL, startUrl + "?morestuff");
        packageInfo.applicationInfo.metaData.putString(SCOPE, startUrl);
        packageInfo.applicationInfo.metaData.putString(WEB_MANIFEST_URL, manifestUrl);
        packageInfo.applicationInfo.metaData.putInt(SHELL_APK_VERSION, SHELL_VERSION);
        packageInfo.applicationInfo.sourceDir = sourceDir;
        return packageInfo;
    }

    // The browser signature is expected to always be the second signature - the first (and any
    // additional ones after the second) are ignored.
    private static PackageInfo newPackageInfoWithBrowserSignature(
            String packageName, Signature signature, String startUrl, String manifestUrl) {
        return newPackageInfo(
                packageName,
                new Signature[] {new Signature(""), signature},
                null,
                startUrl,
                manifestUrl);
    }

    private void addWebApkResolveInfoWithPackageName(
            String startUrl, String packageName, byte[] signature) {
        try {
            Intent intent = Intent.parseUri(startUrl, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(packageName);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(packageName));
            mPackageManager.addPackage(
                    newPackageInfoWithBrowserSignature(
                            packageName, new Signature(signature), startUrl, null));
        } catch (URISyntaxException e) {
            throw new AssertionError("URI is invalid.", e);
        }
    }
}
