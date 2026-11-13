// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.pm.webapp.WebAppInstallRequest;
import android.content.pm.webapp.WebAppManager;
import android.content.pm.webapp.WebAppQueryRequest;
import android.os.Build;

import org.jni_zero.JniUniquePtr;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.function.IntConsumer;
import java.util.function.ObjIntConsumer;

/**
 * Unit tests for {@link TwaInstaller}.
 *
 * <p>Complements {@code TwaInstallerIntegrationTest} by running on Robolectric under {@link
 * Build.VERSION_CODES#CINNAMON_BUN} with a mocked {@link WebAppManager} injected via {@link
 * TwaInstaller#setWebAppManagerForTesting} and mocked JNI natives. Verifies {@link
 * WebAppInstallRequest} and {@link WebAppQueryRequest} construction, install outcome callback
 * mapping (success, user cancellation, and error), and null-service or unsupported-SDK ({@link
 * Build.VERSION_CODES#BAKLAVA}) fallbacks without requiring a live Android 17+ OS service.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
public class TwaInstallerTest {
    private static final String APP_TITLE = "Example TWA";
    private static final String MANIFEST_URL = "https://example.com/manifest.json";
    private static final String PACKAGE_NAME = "com.example.twa";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TwaInstaller.Natives mNativeMock;
    @Mock private JniUniquePtr<TwaInstaller.NativeTwaInstaller> mNativeInstaller;
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
        assertTrue(TwaInstaller.start(mNativeInstaller, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager)
                .install(mInstallRequestCaptor.capture(), any(), mInstallCallbackCaptor.capture());
        assertEquals(APP_TITLE, mInstallRequestCaptor.getValue().getTitle());
        assertEquals(MANIFEST_URL, mInstallRequestCaptor.getValue().getManifestUrl());
        verifyNoInteractions(mNativeMock);
        verify(mNativeInstaller, never()).destroy();

        mInstallCallbackCaptor.getValue().accept(PACKAGE_NAME, WebAppInstallRequest.RESULT_SUCCESS);

        InOrder inOrder = inOrder(mNativeMock, mNativeInstaller);
        inOrder.verify(mNativeMock)
                .onInstallEvent(mNativeInstaller, AddToHomescreenEvent.INSTALL_REQUEST_FINISHED);
        inOrder.verify(mNativeInstaller).destroy();
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_InstallCancelledByUser() {
        assertTrue(TwaInstaller.start(mNativeInstaller, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager)
                .install(mInstallRequestCaptor.capture(), any(), mInstallCallbackCaptor.capture());
        verifyNoInteractions(mNativeMock);
        verify(mNativeInstaller, never()).destroy();

        mInstallCallbackCaptor
                .getValue()
                .accept(PACKAGE_NAME, WebAppInstallRequest.RESULT_CANCELLED_BY_USER);

        InOrder inOrder = inOrder(mNativeMock, mNativeInstaller);
        inOrder.verify(mNativeMock)
                .onInstallEvent(mNativeInstaller, AddToHomescreenEvent.UI_CANCELLED);
        inOrder.verify(mNativeInstaller).destroy();
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_InstallError() {
        assertTrue(TwaInstaller.start(mNativeInstaller, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager)
                .install(mInstallRequestCaptor.capture(), any(), mInstallCallbackCaptor.capture());
        verifyNoInteractions(mNativeMock);
        verify(mNativeInstaller, never()).destroy();

        mInstallCallbackCaptor
                .getValue()
                .accept(PACKAGE_NAME, WebAppInstallRequest.RESULT_INTERNAL_ERROR);

        InOrder inOrder = inOrder(mNativeMock, mNativeInstaller);
        inOrder.verify(mNativeMock)
                .onInstallEvent(mNativeInstaller, AddToHomescreenEvent.INSTALL_FAILED);
        inOrder.verify(mNativeInstaller).destroy();
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_EmptyManifestUrl() {
        assertFalse(TwaInstaller.start(mNativeInstaller, APP_TITLE, ""));

        verify(mWebAppManager, never()).install(any(), any(), any());
        InOrder inOrder = inOrder(mNativeMock, mNativeInstaller);
        inOrder.verify(mNativeMock)
                .onInstallEvent(mNativeInstaller, AddToHomescreenEvent.INSTALL_FAILED);
        inOrder.verify(mNativeInstaller).destroy();
    }

    @Test
    @Feature({"Webapp"})
    public void testStart_WebAppManagerNull() {
        TwaInstaller.setWebAppManagerForTesting(null);

        assertFalse(TwaInstaller.start(mNativeInstaller, APP_TITLE, MANIFEST_URL));

        InOrder inOrder = inOrder(mNativeMock, mNativeInstaller);
        inOrder.verify(mNativeMock)
                .onInstallEvent(mNativeInstaller, AddToHomescreenEvent.INSTALL_FAILED);
        inOrder.verify(mNativeInstaller).destroy();
    }

    @Test
    @Feature({"Webapp"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testStart_UnsupportedSdk() {
        assertFalse(TwaInstaller.start(mNativeInstaller, APP_TITLE, MANIFEST_URL));

        verify(mWebAppManager, never()).install(any(), any(), any());
        InOrder inOrder = inOrder(mNativeMock, mNativeInstaller);
        inOrder.verify(mNativeMock)
                .onInstallEvent(mNativeInstaller, AddToHomescreenEvent.INSTALL_FAILED);
        inOrder.verify(mNativeInstaller).destroy();
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
