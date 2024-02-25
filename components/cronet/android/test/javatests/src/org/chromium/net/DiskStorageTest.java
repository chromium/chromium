// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.CronetTestRule.getTestStorage;
import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;

/** Test CronetEngine disk storage. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "The fallback implementation doesn't support on-disk caches")
public class DiskStorageTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private String mReadOnlyStoragePath;

    @Before
    public void setUp() throws Exception {
        System.loadLibrary("cronet_tests");
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        if (mReadOnlyStoragePath != null) {
            FileUtils.recursivelyDeleteFile(new File(mReadOnlyStoragePath));
        }
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    // Crashing on Android Cronet Builder, see crbug.com/601409.
    public void testReadOnlyStorageDirectory() throws Exception {
        mReadOnlyStoragePath = PathUtils.getDataDirectory() + "/read_only";
        File readOnlyStorage = new File(mReadOnlyStoragePath);
        assertThat(readOnlyStorage.mkdir()).isTrue();
        // Setting the storage directory as readonly has no effect.
        assertThat(readOnlyStorage.setReadOnly()).isTrue();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.setStoragePath(mReadOnlyStoragePath);
                            builder.enableHttpCache(
                                    CronetEngine.Builder.HTTP_CACHE_DISK, 1024 * 1024);
                        });

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        cronetEngine.shutdown();
        FileInputStream newVersionFile = null;
        // Make sure that version file is in readOnlyStoragePath.
        File versionFile = new File(mReadOnlyStoragePath + "/version");
        try {
            newVersionFile = new FileInputStream(versionFile);
            byte[] buffer = new byte[] {0, 0, 0, 0};
            int bytesRead = newVersionFile.read(buffer, 0, 4);
            assertThat(bytesRead).isEqualTo(4);
            assertThat(buffer).isEqualTo(new byte[] {1, 0, 0, 0});
        } finally {
            if (newVersionFile != null) {
                newVersionFile.close();
            }
        }
        File diskCacheDir = new File(mReadOnlyStoragePath + "/disk_cache");
        assertThat(diskCacheDir.exists()).isTrue();
        File prefsDir = new File(mReadOnlyStoragePath + "/prefs");
        assertThat(prefsDir.exists()).isTrue();
    }

    @Test
    @SmallTest
    // Crashing on Android Cronet Builder, see crbug.com/601409.
    public void testPurgeOldVersion() throws Exception {
        String testStorage = getTestStorage(mTestRule.getTestFramework().getContext());
        File versionFile = new File(testStorage + "/version");
        FileOutputStream versionOut = null;
        try {
            versionOut = new FileOutputStream(versionFile);
            versionOut.write(new byte[] {0, 0, 0, 0}, 0, 4);
        } finally {
            if (versionOut != null) {
                versionOut.close();
            }
        }
        File oldPrefsFile = new File(testStorage + "/local_prefs.json");
        FileOutputStream oldPrefsOut = null;
        try {
            oldPrefsOut = new FileOutputStream(oldPrefsFile);
            String sample = "sample content";
            oldPrefsOut.write(sample.getBytes(), 0, sample.length());
        } finally {
            if (oldPrefsOut != null) {
                oldPrefsOut.close();
            }
        }

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.setStoragePath(
                                    getTestStorage(mTestRule.getTestFramework().getContext()));
                            builder.enableHttpCache(
                                    CronetEngine.Builder.HTTP_CACHE_DISK, 1024 * 1024);
                        });

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        cronetEngine.shutdown();
        FileInputStream newVersionFile = null;
        try {
            newVersionFile = new FileInputStream(versionFile);
            byte[] buffer = new byte[] {0, 0, 0, 0};
            int bytesRead = newVersionFile.read(buffer, 0, 4);
            assertThat(bytesRead).isEqualTo(4);
            assertThat(buffer).isEqualTo(new byte[] {1, 0, 0, 0});
        } finally {
            if (newVersionFile != null) {
                newVersionFile.close();
            }
        }
        oldPrefsFile = new File(testStorage + "/local_prefs.json");
        assertThat(!oldPrefsFile.exists()).isTrue();
        File diskCacheDir = new File(testStorage + "/disk_cache");
        assertThat(diskCacheDir.exists()).isTrue();
        File prefsDir = new File(testStorage + "/prefs");
        assertThat(prefsDir.exists()).isTrue();
    }

    @Test
    @SmallTest
    // Tests that if cache version is current, Cronet does not purge the directory.
    public void testCacheVersionCurrent() throws Exception {
        // Initialize a CronetEngine and shut it down.
        ExperimentalCronetEngine.Builder builder =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext());
        builder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1024 * 1024);

        CronetEngine cronetEngine = builder.build();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        cronetEngine.shutdown();

        // Create a sample file in storage directory.
        String testStorage = getTestStorage(mTestRule.getTestFramework().getContext());
        File sampleFile = new File(testStorage + "/sample.json");
        FileOutputStream sampleFileOut = null;
        String sampleContent = "sample content";
        try {
            sampleFileOut = new FileOutputStream(sampleFile);
            sampleFileOut.write(sampleContent.getBytes(), 0, sampleContent.length());
        } finally {
            if (sampleFileOut != null) {
                sampleFileOut.close();
            }
        }

        // Creates a new CronetEngine and make a request.
        CronetEngine engine = builder.build();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        String url2 = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder2 =
                engine.newUrlRequestBuilder(url2, callback2, callback2.getExecutor());
        UrlRequest urlRequest2 = requestBuilder2.build();
        urlRequest2.start();
        callback2.blockForDone();
        assertThat(callback2.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        engine.shutdown();
        // Sample file still exists.
        BufferedReader reader = new BufferedReader(new FileReader(sampleFile));
        StringBuilder stringBuilder = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {
            stringBuilder.append(line);
        }
        reader.close();
        assertThat(stringBuilder.toString()).isEqualTo(sampleContent);
        File diskCacheDir = new File(testStorage + "/disk_cache");
        assertThat(diskCacheDir.exists()).isTrue();
        File prefsDir = new File(testStorage + "/prefs");
        assertThat(prefsDir.exists()).isTrue();
    }

    @Test
    @SmallTest
    // Tests that enableHttpCache throws if storage path not set
    public void testEnableHttpCacheThrowsIfStoragePathNotSet() throws Exception {
        // Initialize a CronetEngine and shut it down.
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            assertThrows(
                                    IllegalArgumentException.class,
                                    () ->
                                            builder.enableHttpCache(
                                                    CronetEngine.Builder.HTTP_CACHE_DISK,
                                                    1024 * 1024));
                        });

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        cronetEngine.shutdown();

        String testStorage = getTestStorage(mTestRule.getTestFramework().getContext());
        File diskCacheDir = new File(testStorage + "/disk_cache");
        assertThat(diskCacheDir.exists()).isFalse();
        File prefsDir = new File(testStorage + "/prefs");
        assertThat(prefsDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    // Tests that prefs file is created even if httpcache isn't enabled
    public void testPrefsFileCreatedWithoutHttpCache() throws Exception {
        // Initialize a CronetEngine and shut it down.
        String testStorage = getTestStorage(mTestRule.getTestFramework().getContext());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch((builder) -> builder.setStoragePath(testStorage));

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        cronetEngine.shutdown();

        File diskCacheDir = new File(testStorage + "/disk_cache");
        assertThat(diskCacheDir.exists()).isFalse();
        File prefsDir = new File(testStorage + "/prefs");
        assertThat(prefsDir.exists()).isTrue();
    }
}
