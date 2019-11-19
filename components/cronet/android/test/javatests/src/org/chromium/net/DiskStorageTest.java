// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.getContext;
import static org.chromium.net.CronetTestRule.getTestStorage;

import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.util.Arrays;

/**
 * Test CronetEngine disk storage.
 */
@RunWith(AndroidJUnit4.class)
public class DiskStorageTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private String mReadOnlyStoragePath;

    @Before
    public void setUp() throws Exception {
        System.loadLibrary("cronet_tests");
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
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
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Crashing on Android Cronet Builder, see crbug.com/601409.
    public void testReadOnlyStorageDirectory() throws Exception {
        mReadOnlyStoragePath = PathUtils.getDataDirectory() + "/read_only";
        File readOnlyStorage = new File(mReadOnlyStoragePath);
        assertTrue(readOnlyStorage.mkdir());
        // Setting the storage directory as readonly has no effect.
        assertTrue(readOnlyStorage.setReadOnly());
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.setStoragePath(mReadOnlyStoragePath);
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1024 * 1024);

        CronetEngine cronetEngine = builder.build();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        cronetEngine.shutdown();
        FileInputStream newVersionFile = null;
        // Make sure that version file is in readOnlyStoragePath.
        File versionFile = new File(mReadOnlyStoragePath + "/version");
        try {
            newVersionFile = new FileInputStream(versionFile);
            byte[] buffer = new byte[] {0, 0, 0, 0};
            int bytesRead = newVersionFile.read(buffer, 0, 4);
            assertEquals(4, bytesRead);
            assertTrue(Arrays.equals(new byte[] {1, 0, 0, 0}, buffer));
        } finally {
            if (newVersionFile != null) {
                newVersionFile.close();
            }
        }
        File diskCacheDir = new File(mReadOnlyStoragePath + "/disk_cache");
        assertTrue(diskCacheDir.exists());
        File prefsDir = new File(mReadOnlyStoragePath + "/prefs");
        assertTrue(prefsDir.exists());
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Crashing on Android Cronet Builder, see crbug.com/601409.
    public void testPurgeOldVersion() throws Exception {
        String testStorage = getTestStorage(getContext());
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
            String dummy = "dummy content";
            oldPrefsOut.write(dummy.getBytes(), 0, dummy.length());
        } finally {
            if (oldPrefsOut != null) {
                oldPrefsOut.close();
            }
        }

        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.setStoragePath(getTestStorage(getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1024 * 1024);

        CronetEngine cronetEngine = builder.build();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        cronetEngine.shutdown();
        FileInputStream newVersionFile = null;
        try {
            newVersionFile = new FileInputStream(versionFile);
            byte[] buffer = new byte[] {0, 0, 0, 0};
            int bytesRead = newVersionFile.read(buffer, 0, 4);
            assertEquals(4, bytesRead);
            assertTrue(Arrays.equals(new byte[] {1, 0, 0, 0}, buffer));
        } finally {
            if (newVersionFile != null) {
                newVersionFile.close();
            }
        }
        oldPrefsFile = new File(testStorage + "/local_prefs.json");
        assertTrue(!oldPrefsFile.exists());
        File diskCacheDir = new File(testStorage + "/disk_cache");
        assertTrue(diskCacheDir.exists());
        File prefsDir = new File(testStorage + "/prefs");
        assertTrue(prefsDir.exists());
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that if cache version is current, Cronet does not purge the directory.
    public void testCacheVersionCurrent() throws Exception {
        // Initialize a CronetEngine and shut it down.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.setStoragePath(getTestStorage(getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1024 * 1024);

        CronetEngine cronetEngine = builder.build();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = requestBuilder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        cronetEngine.shutdown();

        // Create a dummy file in storage directory.
        String testStorage = getTestStorage(getContext());
        File dummyFile = new File(testStorage + "/dummy.json");
        FileOutputStream dummyFileOut = null;
        String dummyContent = "dummy content";
        try {
            dummyFileOut = new FileOutputStream(dummyFile);
            dummyFileOut.write(dummyContent.getBytes(), 0, dummyContent.length());
        } finally {
            if (dummyFileOut != null) {
                dummyFileOut.close();
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
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        engine.shutdown();
        // Dummy file still exists.
        BufferedReader reader = new BufferedReader(new FileReader(dummyFile));
        StringBuilder stringBuilder = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {
            stringBuilder.append(line);
        }
        reader.close();
        assertEquals(dummyContent, stringBuilder.toString());
        File diskCacheDir = new File(testStorage + "/disk_cache");
        assertTrue(diskCacheDir.exists());
        File prefsDir = new File(testStorage + "/prefs");
        assertTrue(prefsDir.exists());
    }
}
