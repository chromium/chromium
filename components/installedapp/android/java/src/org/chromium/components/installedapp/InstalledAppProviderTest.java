// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.installedapp;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.util.Pair;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.installedapp.mojom.RelatedApplication;
import org.chromium.url.GURL;
import org.chromium.url.mojom.Url;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/** Ensure that the InstalledAppProvider returns the correct apps. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class InstalledAppProviderTest {
    private static final String ASSET_STATEMENTS_KEY =
            InstalledAppProviderImpl.ASSET_STATEMENTS_KEY;
    private static final String RELATION_HANDLE_ALL_URLS =
            "delegate_permission/common.handle_all_urls";
    private static final String NAMESPACE_WEB =
            InstalledAppProviderImpl.ASSET_STATEMENT_NAMESPACE_WEB;
    private static final String PLATFORM_ANDROID =
            InstalledAppProviderImpl.RELATED_APP_PLATFORM_ANDROID;
    private static final String PLATFORM_WEBAPP =
            InstalledAppProviderImpl.RELATED_APP_PLATFORM_WEBAPP;
    private static final int MAX_ALLOWED_RELATED_APPS =
            InstalledAppProviderImpl.MAX_ALLOWED_RELATED_APPS;
    private static final String PLATFORM_OTHER = "itunes";
    // Note: Android package name and origin deliberately unrelated (there is no requirement that
    // they be the same).
    private static final String PACKAGE_NAME_1 = "com.app1.package";
    private static final String PACKAGE_NAME_2 = "com.app2.package";
    private static final String PACKAGE_NAME_3 = "com.app3.package";
    private static final String URL_UNRELATED = "https://appstore.example.com/app1";
    private static final String ORIGIN = "https://example.com:8000";
    private static final String URL_ON_ORIGIN =
            "https://example.com:8000/path/to/page.html?key=value#fragment";
    private static final String MANIFEST_URL = "https://example.com:8000/manifest.json";
    private static final String OTHER_MANIFEST_URL = "https://example2.com:8000/manifest.json";
    private static final String OTHER_MANIFEST_URL3 = "https://example3.com:8000/manifest.json";
    private static final String OTHER_MANIFEST_URL4 = "https://example4.com:8000/manifest.json";
    private static final String ORIGIN_SYNTAX_ERROR = "https:{";
    private static final String ORIGIN_MISSING_SCHEME = "path/only";
    private static final String ORIGIN_MISSING_HOST = "file:///path/piece";
    private static final String ORIGIN_MISSING_PORT = "http://example.com";
    private static final String ORIGIN_DIFFERENT_SCHEME = "http://example.com:8000";
    private static final String ORIGIN_DIFFERENT_HOST = "https://example.org:8000";
    private static final String ORIGIN_DIFFERENT_PORT = "https://example.com:8001";

    @Rule public JniMocker mocker = new JniMocker();

    @Mock private MockRenderFrameHost mMockRenderFrameHost;
    private FakePackageManager mFakePackageManager;
    private InstalledAppProviderTestImpl mInstalledAppProvider;
    private FakeInstantAppsHandler mFakeInstantAppsHandler;
    private TestInstalledAppProviderImplJni mTestInstalledAppProviderImplJni;

    private static class FakePackageManager extends PackageManagerDelegate {
        private Map<String, PackageInfo> mPackageInfo = new HashMap<>();
        private Map<String, Resources> mResources = new HashMap<>();

        // The set of installed WebAPKs identified by their manifest URL.
        private Set<String> mInstalledWebApks = new HashSet<>();

        public void addPackageInfo(PackageInfo packageInfo) {
            mPackageInfo.put(packageInfo.packageName, packageInfo);
        }

        public void addResources(String packageName, Resources resources) {
            mResources.put(packageName, resources);
        }

        public void addWebApk(String manifestUrl) {
            mInstalledWebApks.add(manifestUrl);
        }

        public boolean isWebApkInstalled(String manifestUrl) {
            return mInstalledWebApks.contains(manifestUrl);
        }

        @Override
        public ApplicationInfo getApplicationInfo(String packageName, int flags)
                throws NameNotFoundException {
            return getPackageInfo(packageName, flags).applicationInfo;
        }

        @Override
        public Resources getResourcesForApplication(ApplicationInfo appInfo)
                throws NameNotFoundException {
            if (!mResources.containsKey(appInfo.packageName)) throw new NameNotFoundException();
            return mResources.get(appInfo.packageName);
        }

        @Override
        public PackageInfo getPackageInfo(String packageName, int flags)
                throws NameNotFoundException {
            if (!mPackageInfo.containsKey(packageName)) throw new NameNotFoundException();
            return mPackageInfo.get(packageName);
        }
    }

    private class InstalledAppProviderTestImpl extends InstalledAppProviderImpl {
        public InstalledAppProviderTestImpl(
                RenderFrameHost renderFrameHost, FakeInstantAppsHandler instantAppsHandler) {
            super(
                    new BrowserContextHandle() {
                        @Override
                        public long getNativeBrowserContextPointer() {
                            return 1;
                        }
                    },
                    renderFrameHost,
                    instantAppsHandler::isInstantAppAvailable);
        }

        @Override
        public boolean isWebApkInstalled(String manifestUrl) {
            return mFakePackageManager.isWebApkInstalled(manifestUrl);
        }
    }

    private static class TestInstalledAppProviderImplJni
            implements InstalledAppProviderImpl.Natives {
        private final Map<String, String> mRelationMap = new HashMap<>();
        private ArrayList<Pair<Callback<Boolean>, Boolean>> mCallbacks;

        public void addVerfication(String webDomain, String manifestUrl) {
            mRelationMap.put(webDomain, manifestUrl);
        }

        @Override
        public void checkDigitalAssetLinksRelationshipForWebApk(
                BrowserContextHandle browserContextHandle,
                String webDomain,
                String manifestUrl,
                Callback<Boolean> callback) {
            boolean result =
                    mRelationMap.containsKey(webDomain)
                            && mRelationMap.get(webDomain).equals(manifestUrl);
            if (mCallbacks == null) {
                callback.onResult(result);
                return;
            }

            mCallbacks.add(Pair.create(callback, result));
            if (mCallbacks.size() == 3) {
                Pair<Callback<Boolean>, Boolean> stashed = mCallbacks.get(1);
                stashed.first.onResult(stashed.second);
                stashed = mCallbacks.get(0);
                stashed.first.onResult(stashed.second);
                stashed = mCallbacks.get(2);
                stashed.first.onResult(stashed.second);
            }
        }

        // If called, the callbacks passed to {@link checkDigitalAssetLinksRelationshipForWebApk}
        // won't be executed until it's been called three times, and then they'll be executed out of
        // order.
        void rearrangeOrderOfResults() {
            mCallbacks = new ArrayList<>();
        }
    }

    /**
     * FakeInstantAppsHandler lets us mock getting RelatedApplications from a URL in the absence of
     * proper GMSCore calls.
     */
    private static class FakeInstantAppsHandler {
        private final List<Pair<String, Boolean>> mRelatedApplicationList;

        public FakeInstantAppsHandler() {
            mRelatedApplicationList = new ArrayList<Pair<String, Boolean>>();
        }

        public void addInstantApp(String url, boolean holdback) {
            mRelatedApplicationList.add(Pair.create(url, holdback));
        }

        public void resetForTest() {
            mRelatedApplicationList.clear();
        }

        // TODO(thildebr): When the implementation of isInstantAppAvailable is complete, we need to
        // test its functionality instead of stubbing it out here. Instead we can create a wrapper
        // around the GMSCore functionality we need and override that here instead.
        public boolean isInstantAppAvailable(
                String url, boolean checkHoldback, boolean includeUserPrefersBrowser) {
            for (Pair<String, Boolean> pair : mRelatedApplicationList) {
                if (url.startsWith(pair.first) && checkHoldback == pair.second) {
                    return true;
                }
            }
            return false;
        }
    }

    /**
     * Helper function allows for the "installation" of Android package names and setting up
     * Resources for installed packages.
     */
    private void setMetaDataAndResourcesForTest(
            String packageName, Bundle metaData, Resources resources) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = packageName;
        packageInfo.applicationInfo.metaData = metaData;

        mFakePackageManager.addPackageInfo(packageInfo);
        mFakePackageManager.addResources(packageName, resources);
    }

    /**
     * Fakes the Resources object, allowing lookup of a single String value.
     *
     * <p>Note: The real Resources object defines a mapping to many values. This fake object only
     * allows a single value in the mapping, and it must be a String (which is all that is required
     * for these tests).
     */
    private static class FakeResources extends Resources {
        private static AssetManager sAssetManager = createAssetManager();
        private final int mId;
        private final String mValue;

        private static AssetManager createAssetManager() {
            try {
                return AssetManager.class.getConstructor().newInstance();
            } catch (Exception e) {
                return null;
            }
        }

        // Do not warn about deprecated call to Resources(); the documentation says code is not
        // supposed to create its own Resources object, but we are using it to fake out the
        // Resources, and there is no other way to do that.
        @SuppressWarnings("deprecation")
        public FakeResources(int identifier, String value) {
            super(sAssetManager, null, null);
            mId = identifier;
            mValue = value;
        }

        @Override
        public int getIdentifier(String name, String defType, String defPackage) {
            if (name == null) throw new NullPointerException();

            // There is *no guarantee* (in the Digital Asset Links spec) about what the string
            // resource should be called ("asset_statements" is just an example). Therefore,
            // getIdentifier cannot be used to get the asset statements string. Always fail the
            // lookup here, to ensure the implementation isn't relying on any particular hard-coded
            // string.
            return 0;
        }

        @Override
        public String getString(int id) {
            if (id != mId) {
                throw new Resources.NotFoundException("id 0x" + Integer.toHexString(id));
            }

            return mValue;
        }
    }

    /** Creates a metaData bundle with a single resource-id key. */
    private static Bundle createMetaData(String metaDataName, int metaDataResourceId) {
        Bundle metaData = new Bundle();
        metaData.putInt(metaDataName, metaDataResourceId);
        return metaData;
    }

    /**
     * Sets a resource with a single key-value pair in an Android package's manifest.
     *
     * <p>The value is always a string.
     */
    private void setStringResource(String packageName, String key, String value) {
        int identifier = 0x1234;
        Bundle metaData = createMetaData(key, identifier);
        FakeResources resources = new FakeResources(identifier, value);
        setMetaDataAndResourcesForTest(packageName, metaData, resources);
    }

    /** Creates a valid Android asset statement string. */
    private String createAssetStatement(String platform, String relation, String url) {
        return String.format(
                "{\"relation\": [\"%s\"], \"target\": {\"namespace\": \"%s\", \"site\": \"%s\"}}",
                relation, platform, url);
    }

    /**
     * Sets an asset statement to an Android package's manifest (in the fake package manager).
     *
     * <p>Only one asset statement can be set for a given package (if this is called twice on the
     * same package, overwrites the previous asset statement).
     *
     * <p>This corresponds to a Statement List in the Digital Asset Links spec v1.
     */
    private void setAssetStatement(
            String packageName, String platform, String relation, String url) {
        String statements = "[" + createAssetStatement(platform, relation, url) + "]";
        setStringResource(packageName, ASSET_STATEMENTS_KEY, statements);
    }

    /** Creates a RelatedApplication to put in the web app manifest. */
    private RelatedApplication createRelatedApplication(String platform, String id, String url) {
        RelatedApplication application = new RelatedApplication();
        application.platform = platform;
        application.id = id;
        application.url = url;
        return application;
    }

    /**
     * Calls filterInstalledApps with the given inputs, and tests that the expected result is
     * returned.
     */
    private void verifyInstalledApps(
            RelatedApplication[] manifestRelatedApps,
            RelatedApplication[] expectedInstalledRelatedApps)
            throws Exception {
        final AtomicBoolean called = new AtomicBoolean(false);
        Url manifestUrl = new Url();
        manifestUrl.url = MANIFEST_URL;

        mInstalledAppProvider.filterInstalledApps(
                manifestRelatedApps,
                manifestUrl,
                new InstalledAppProvider.FilterInstalledApps_Response() {
                    @Override
                    public void call(RelatedApplication[] installedRelatedApps) {
                        Assert.assertEquals(
                                expectedInstalledRelatedApps.length, installedRelatedApps.length);

                        for (int i = 0; i < installedRelatedApps.length; i++) {
                            Assert.assertEquals(
                                    expectedInstalledRelatedApps[i], installedRelatedApps[i]);
                        }
                        called.set(true);
                    }
                });
        CriteriaHelper.pollUiThreadNested(() -> called.get());
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mTestInstalledAppProviderImplJni = new TestInstalledAppProviderImplJni();
        mocker.mock(InstalledAppProviderImplJni.TEST_HOOKS, mTestInstalledAppProviderImplJni);

        GURL urlOnOrigin = new GURL(URL_ON_ORIGIN);
        Mockito.when(mMockRenderFrameHost.getLastCommittedURL()).thenReturn(urlOnOrigin);

        mFakePackageManager = new FakePackageManager();
        mFakeInstantAppsHandler = new FakeInstantAppsHandler();
        mInstalledAppProvider =
                new InstalledAppProviderTestImpl(mMockRenderFrameHost, mFakeInstantAppsHandler);
        mInstalledAppProvider.setPackageManagerDelegateForTest(mFakePackageManager);
    }

    /** Origin of the page using the API is missing certain parts of the URI. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOriginMissingParts() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};

        GURL originMissingScheme = new GURL(ORIGIN_MISSING_SCHEME);
        Mockito.when(mMockRenderFrameHost.getLastCommittedURL()).thenReturn(originMissingScheme);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        GURL originMissingHost = new GURL(ORIGIN_MISSING_HOST);
        Mockito.when(mMockRenderFrameHost.getLastCommittedURL()).thenReturn(originMissingHost);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * No related Android apps.
     *
     * <p>An Android app relates to the web app, but not mutual.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testNoRelatedApps() throws Exception {
        // The web manifest has no related apps.
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {};

        // One Android app is installed named |PACKAGE_NAME_1|. It has a related web app with origin
        // |ORIGIN|.
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app with no id (package name).
     *
     * <p>An Android app relates to the web app, but not mutual.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedAppNoId() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {createRelatedApplication(PLATFORM_ANDROID, null, null)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related app (from a non-Android platform).
     *
     * <p>An Android app with the same id relates to the web app. This should be ignored since the
     * manifest doesn't mention the Android app.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedNonAndroidApp() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_OTHER, PACKAGE_NAME_1, null)
                };

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is not installed.
     *
     * <p>Another Android app relates to the web app, but not mutual.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedAppNotInstalled() throws Exception {
        // The web manifest has a related Android app named |PACKAGE_NAME_1|.
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        // One Android app is installed named |PACKAGE_NAME_2|. It has a related web app with origin
        // |ORIGIN|.
        setAssetStatement(PACKAGE_NAME_2, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app manifest has an asset_statements key, but the resource it links to is missing. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedAppBrokenAssetStatementsResource() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        Bundle metaData = createMetaData(ASSET_STATEMENTS_KEY, 0x1234);
        String statements =
                "[" + createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN) + "]";
        FakeResources resources = new FakeResources(0x4321, statements);
        setMetaDataAndResourcesForTest(PACKAGE_NAME_1, metaData, resources);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is not mutually related (has no asset_statements). */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedAppNoAssetStatements() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setStringResource(PACKAGE_NAME_1, null, null);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is not mutually related (has no asset_statements). */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedAppNoAssetStatementsNullMetadata() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        FakeResources resources = new FakeResources(0x4321, null);
        setMetaDataAndResourcesForTest(PACKAGE_NAME_1, null, resources);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is related to other origins.
     *
     * <p>Tests three cases: - The Android app is related to a web app with a different scheme. -
     * The Android app is related to a web app with a different host. - The Android app is related
     * to a web app with a different port.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneRelatedAppRelatedToDifferentOrigins() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_SCHEME);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_HOST);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_PORT);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is installed and mutually related. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneInstalledRelatedApp() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Change the frame URL and ensure the app relates to the new URL, not the old one.
     *
     * <p>This simulates navigating the frame while keeping the same Mojo service open.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testDynamicallyChangingUrl() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_SCHEME);

        // Should be empty, since Android app does not relate to this frame's origin.
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        // Simulate a navigation to a different origin.
        GURL originDifferentScheme = new GURL(ORIGIN_DIFFERENT_SCHEME);
        Mockito.when(mMockRenderFrameHost.getLastCommittedURL()).thenReturn(originDifferentScheme);

        // Now the result should include the Android app that relates to the new origin.
        expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        // Simulate the native RenderFrameHost disappearing.
        Mockito.when(mMockRenderFrameHost.getLastCommittedURL()).thenReturn(null);

        expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app (installed and mutually related), with a non-null URL field. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInstalledRelatedAppWithUrl() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, URL_UNRELATED)
                };

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is related to multiple origins. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testMultipleAssetStatements() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        // Create an asset_statements field with multiple statements. The second one matches the web
        // app.
        String statements =
                "["
                        + createAssetStatement(
                                NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_HOST)
                        + ", "
                        + createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN)
                        + "]";
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** A JSON syntax error in the Android app's asset statement. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementSyntaxError() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statements = "[{\"target\" {}}]";
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** The Android app's asset statement is not an array. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementNotArray() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statement = createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statement);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** The Android app's asset statement array contains non-objects. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementArrayNoObjects() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statements =
                "["
                        + createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN)
                        + ", 4]";
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        // Expect it to ignore the integer and successfully parse the valid object.
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Android app has no "relation" in the asset statement.
     *
     * <p>Currently, the relation string (in the Android package's asset statement) is ignored, so
     * the app is still returned as "installed".
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementNoRelation() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statements =
                String.format(
                        "[{\"target\": {\"namespace\": \"%s\", \"site\": \"%s\"}}]",
                        NAMESPACE_WEB, ORIGIN);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        // TODO(mgiuca): [Spec issue] Should we require a specific relation string, rather than any
        // or no relation?
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Android app is related with a non-standard relation.
     *
     * <p>Currently, the relation string (in the Android package's asset statement) is ignored, so
     * any will do. Is this desirable, or do we want to require a specific relation string?
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementNonStandardRelation() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, "nonstandard/relation", ORIGIN);

        // TODO(mgiuca): [Spec issue] Should we require a specific relation string, rather than any
        // or no relation?
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has no "target" in the asset statement. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementNoTarget() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statements = String.format("[{\"relation\": [\"%s\"]}]", RELATION_HANDLE_ALL_URLS);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has no "namespace" in the asset statement. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementNoNamespace() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statements =
                String.format(
                        "[{\"relation\": [\"%s\"], \"target\": {\"site\": \"%s\"}}]",
                        RELATION_HANDLE_ALL_URLS, ORIGIN);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app is related, but not to the web namespace. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testNonWebAssetStatement() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(PACKAGE_NAME_1, "play", RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has no "site" in the asset statement. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementNoSite() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String statements =
                String.format(
                        "[{\"relation\": [\"%s\"], \"target\": {\"namespace\": \"%s\"}}]",
                        RELATION_HANDLE_ALL_URLS, NAMESPACE_WEB);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has a syntax error in the "site" field of the asset statement. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementSiteSyntaxError() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_SYNTAX_ERROR);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has a "site" field missing certain parts of the URI (scheme, host, port). */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementSiteMissingParts() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_MISSING_SCHEME);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_MISSING_HOST);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_MISSING_PORT);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is related with a path part in the "site" field.
     *
     * <p>The path part shouldn't really be there (according to the Digital Asset Links spec), but
     * if it is, we are lenient and just ignore it (matching only the origin).
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testAssetStatementSiteHasPath() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        String site = ORIGIN + "/path";
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, site);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is installed and mutually related.
     *
     * <p>Another Android app relates to the web app, but not mutual.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testExtraInstalledApp() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        setAssetStatement(PACKAGE_NAME_2, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Two related Android apps; Android apps both installed and mutually related.
     *
     * <p>Web app also related to an app with the same name on another platform, and another Android
     * app which is not installed.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testMultipleInstalledRelatedApps() throws Exception {
        RelatedApplication[] manifestRelatedApps =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null),
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_2, null),
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_3, null)
                };

        setAssetStatement(PACKAGE_NAME_2, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        setAssetStatement(PACKAGE_NAME_3, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps =
                new RelatedApplication[] {manifestRelatedApps[1], manifestRelatedApps[2]};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Tests the pseudo-random artificial delay to counter a timing attack. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testArtificialDelay() throws Exception {
        byte[] salt = {
            0x64, 0x09, -0x68, -0x25, 0x70, 0x11, 0x25, 0x24, 0x68, -0x1a, 0x08, 0x79, -0x12, -0x50,
            0x3b, -0x57, -0x17, -0x4d, 0x46, 0x02
        };
        PackageHash.setGlobalSaltForTesting(salt);
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        // Installed app.
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)
                };
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
        // This expectation is based on HMAC_SHA256(salt, packageName encoded in UTF-8), taking the
        // low 10 bits of the first two bytes of the result / 100.
        Assert.assertEquals(2, mInstalledAppProvider.mLastDelayForTesting);

        // Non-installed app.
        manifestRelatedApps =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_2, null)
                };
        expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
        // This expectation is based on HMAC_SHA256(salt, packageName encoded in UTF-8), taking the
        // low 10 bits of the first two bytes of the result / 100.
        Assert.assertEquals(5, mInstalledAppProvider.mLastDelayForTesting);

        // Own WebAPK.
        manifestRelatedApps =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_WEBAPP, null, MANIFEST_URL)
                };
        expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
        // This expectation is based on HMAC_SHA256(salt, manifestUrl encoded in UTF-8), taking the
        // low 10 bits of the first two bytes of the result / 100.
        Assert.assertEquals(3, mInstalledAppProvider.mLastDelayForTesting);

        // Another WebAPK.
        manifestRelatedApps =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_WEBAPP, null, OTHER_MANIFEST_URL)
                };
        expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
        // This expectation is based on HMAC_SHA256(salt, manifestUrl encoded in UTF-8), taking the
        // low 10 bits of the first two bytes of the result / 100.
        Assert.assertEquals(8, mInstalledAppProvider.mLastDelayForTesting);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMultipleAppsIncludingInstantApps() throws Exception {
        RelatedApplication[] manifestRelatedApps =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null),
                    // Instant Apps:
                    createRelatedApplication(
                            PLATFORM_ANDROID,
                            InstalledAppProviderImpl.INSTANT_APP_ID_STRING,
                            ORIGIN),
                    createRelatedApplication(
                            PLATFORM_ANDROID,
                            InstalledAppProviderImpl.INSTANT_APP_HOLDBACK_ID_STRING,
                            ORIGIN)
                };

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        mFakeInstantAppsHandler.addInstantApp(ORIGIN, true);

        RelatedApplication[] expectedInstalledRelatedApps =
                new RelatedApplication[] {manifestRelatedApps[0], manifestRelatedApps[2]};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Multiple related uninstalled apps (over the allowed limit) followed by one related Android
     * app which is installed and mutually related.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testRelatedAppsOverAllowedThreshold() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[MAX_ALLOWED_RELATED_APPS + 1];
        for (int i = 0; i < MAX_ALLOWED_RELATED_APPS; i++) {
            manifestRelatedApps[i] =
                    createRelatedApplication(
                            PLATFORM_ANDROID, PACKAGE_NAME_2 + String.valueOf(i), null);
        }
        manifestRelatedApps[MAX_ALLOWED_RELATED_APPS] =
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null);
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        // Although the app is installed, and verifiable, it was included after the maximum allowed
        // number of related apps.
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Check that a website can find its own WebAPK when installed. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInstalledWebApkForWebsite() throws Exception {
        RelatedApplication webApk = createRelatedApplication(PLATFORM_WEBAPP, null, MANIFEST_URL);
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {webApk};
        mFakePackageManager.addWebApk(MANIFEST_URL);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {webApk};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Check that a website can find another WebAPK when installed & verfied. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInstalledWebApkForOtherWebsite() throws Exception {
        RelatedApplication webApk =
                createRelatedApplication(PLATFORM_WEBAPP, null, OTHER_MANIFEST_URL);
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {webApk};
        mFakePackageManager.addWebApk(OTHER_MANIFEST_URL);

        verifyInstalledApps(manifestRelatedApps, new RelatedApplication[] {});

        mTestInstalledAppProviderImplJni.addVerfication(OTHER_MANIFEST_URL, MANIFEST_URL);
        verifyInstalledApps(manifestRelatedApps, new RelatedApplication[] {webApk});
    }

    /** Check that a website can query another WebAPK when not installed but verfied. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInstalledWebApkForOtherWebsiteNotInstalled() throws Exception {
        RelatedApplication webApk =
                createRelatedApplication(PLATFORM_WEBAPP, null, OTHER_MANIFEST_URL);
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {webApk};

        mTestInstalledAppProviderImplJni.addVerfication(MANIFEST_URL, OTHER_MANIFEST_URL);
        verifyInstalledApps(manifestRelatedApps, new RelatedApplication[] {});
    }

    /**
     * Tests that the order of returned filtered apps matches the order of the apps passed to {@link
     * filterInstalledApps}.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOrderOfResults() throws Exception {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {
                    createRelatedApplication(PLATFORM_WEBAPP, null, OTHER_MANIFEST_URL),
                    createRelatedApplication(PLATFORM_WEBAPP, null, OTHER_MANIFEST_URL3),
                    createRelatedApplication(PLATFORM_WEBAPP, null, OTHER_MANIFEST_URL4)
                };
        mFakePackageManager.addWebApk(OTHER_MANIFEST_URL);
        mFakePackageManager.addWebApk(OTHER_MANIFEST_URL3);
        mFakePackageManager.addWebApk(OTHER_MANIFEST_URL4);
        mTestInstalledAppProviderImplJni.addVerfication(OTHER_MANIFEST_URL, MANIFEST_URL);
        mTestInstalledAppProviderImplJni.addVerfication(OTHER_MANIFEST_URL3, MANIFEST_URL);
        mTestInstalledAppProviderImplJni.addVerfication(OTHER_MANIFEST_URL4, MANIFEST_URL);

        mTestInstalledAppProviderImplJni.rearrangeOrderOfResults();
        verifyInstalledApps(manifestRelatedApps, manifestRelatedApps);
    }
}
