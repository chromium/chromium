// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.pm.webapp.WebAppInstallRequest;
import android.content.pm.webapp.WebAppManager;
import android.content.pm.webapp.WebAppQueryRequest;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.function.IntConsumer;
import java.util.function.ObjIntConsumer;

/** Unit tests for {@link TwaInstaller}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
public class TwaInstallerTest {
    private static final long NATIVE_INSTALLER = 42L;
    private static final String APP_TITLE = "Example TWA";
    private static final String MANIFEST_URL = "https://example.com/manifest.json";
    private static final String PACKAGE_NAME = "com.example.twa";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TwaInstaller.Natives mNativeMock;
    @Mock private WebAppManager mWebAppManager;

    @Captor private ArgumentCaptor<WebAppInstallRequest> mInstallRequestCaptor;
    @Captor private ArgumentCaptor<ObjIntConsumer<String>> mInstallCallbackCaptor;
    @Captor private ArgumentCaptor<WebAppQueryRequest> mQueryRequestCaptor;
    @Captor private ArgumentCaptor<IntConsumer> mQueryCallbackCaptor;

    @Before
    public void setUp() {
        TwaInstallerJni.setInstanceForTesting(mNativeMock);
        TwaInstaller.setWebAppManagerForTesting(mWebAppManager);
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_InstallSuccess() {
        assertTrue(TwaInstaller.start(NATIVE_INSTALLER, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager)
                .install(mInstallRequestCaptor.capture(), any(), mInstallCallbackCaptor.capture());
        assertEquals(APP_TITLE, mInstallRequestCaptor.getValue().getTitle());
        assertEquals(MANIFEST_URL, mInstallRequestCaptor.getValue().getManifestUrl());
        verifyNoInteractions(mNativeMock);

        mInstallCallbackCaptor.getValue().accept(PACKAGE_NAME, WebAppInstallRequest.RESULT_SUCCESS);

        verify(mNativeMock)
                .onInstallEvent(NATIVE_INSTALLER, AddToHomescreenEvent.INSTALL_REQUEST_FINISHED);
        verify(mNativeMock).destroy(NATIVE_INSTALLER);
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_InstallCancelledByUser() {
        assertTrue(TwaInstaller.start(NATIVE_INSTALLER, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager)
                .install(mInstallRequestCaptor.capture(), any(), mInstallCallbackCaptor.capture());
        mInstallCallbackCaptor
                .getValue()
                .accept(PACKAGE_NAME, WebAppInstallRequest.RESULT_CANCELLED_BY_USER);

        verify(mNativeMock).onInstallEvent(NATIVE_INSTALLER, AddToHomescreenEvent.UI_CANCELLED);
        verify(mNativeMock).destroy(NATIVE_INSTALLER);
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_InstallError() {
        assertTrue(TwaInstaller.start(NATIVE_INSTALLER, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager)
                .install(mInstallRequestCaptor.capture(), any(), mInstallCallbackCaptor.capture());
        mInstallCallbackCaptor
                .getValue()
                .accept(PACKAGE_NAME, WebAppInstallRequest.RESULT_INTERNAL_ERROR);

        verify(mNativeMock).onInstallEvent(NATIVE_INSTALLER, AddToHomescreenEvent.INSTALL_FAILED);
        verify(mNativeMock).destroy(NATIVE_INSTALLER);
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_EmptyManifestUrl() {
        assertFalse(TwaInstaller.start(NATIVE_INSTALLER, APP_TITLE, ""));

        verify(mWebAppManager, never()).install(any(), any(), any());
        verify(mNativeMock).onInstallEvent(NATIVE_INSTALLER, AddToHomescreenEvent.INSTALL_FAILED);
        verify(mNativeMock).destroy(NATIVE_INSTALLER);
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_WebAppManagerNull() {
        TwaInstaller.setWebAppManagerForTesting(null);

        assertFalse(TwaInstaller.start(NATIVE_INSTALLER, APP_TITLE, MANIFEST_URL));

        verify(mNativeMock).onInstallEvent(NATIVE_INSTALLER, AddToHomescreenEvent.INSTALL_FAILED);
        verify(mNativeMock).destroy(NATIVE_INSTALLER);
    }

    @Test
    @Feature({"Webapp"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testStart_UnsupportedSdk() {
        assertFalse(TwaInstaller.start(NATIVE_INSTALLER, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager, never()).install(any(), any(), any());
        verify(mNativeMock).onInstallEvent(NATIVE_INSTALLER, AddToHomescreenEvent.INSTALL_FAILED);
        verify(mNativeMock).destroy(NATIVE_INSTALLER);
    }

    @Test
    @Feature({"Webapp"})
    public void testIsInstalled_Installed() {
        Promise<Boolean> promise = TwaInstaller.isInstalled(APP_TITLE);
        assertFalse(promise.isFulfilled());

        verify(mWebAppManager)
                .query(mQueryRequestCaptor.capture(), any(), mQueryCallbackCaptor.capture());
        assertEquals(APP_TITLE, mQueryRequestCaptor.getValue().getPackageName());

        mQueryCallbackCaptor.getValue().accept(WebAppQueryRequest.RESULT_INSTALLED);
        assertTrue(promise.isFulfilled());
        assertTrue(promise.getResult());
    }

    @Test
    @Feature({"Webapp"})
    public void testIsInstalled_NotInstalled() {
        Promise<Boolean> promise = TwaInstaller.isInstalled(APP_TITLE);
        assertFalse(promise.isFulfilled());

        verify(mWebAppManager)
                .query(mQueryRequestCaptor.capture(), any(), mQueryCallbackCaptor.capture());
        mQueryCallbackCaptor.getValue().accept(WebAppQueryRequest.RESULT_NOT_INSTALLED);
        assertTrue(promise.isFulfilled());
        assertFalse(promise.getResult());
    }

    @Test
    @Feature({"Webapp"})
    public void testIsInstalled_WebAppManagerNull() {
        TwaInstaller.setWebAppManagerForTesting(null);

        Promise<Boolean> promise = TwaInstaller.isInstalled(APP_TITLE);
        assertTrue(promise.isFulfilled());
        assertFalse(promise.getResult());
    }

    @Test
    @Feature({"Webapp"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testIsInstalled_UnsupportedSdk() {
        Promise<Boolean> promise = TwaInstaller.isInstalled(APP_TITLE);
        assertTrue(promise.isFulfilled());
        assertFalse(promise.getResult());
        verify(mWebAppManager, never()).query(any(), any(), any());
    }
}
