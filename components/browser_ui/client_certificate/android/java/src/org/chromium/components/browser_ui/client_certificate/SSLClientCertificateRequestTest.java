// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.client_certificate;

import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.ActivityNotFoundException;
import android.security.KeyChainAliasCallback;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.client_certificate.SSLClientCertificateRequest.CertSelectionFailureDialog;
import org.chromium.components.browser_ui.client_certificate.SSLClientCertificateRequest.KeyChainCertSelectionWrapper;

/** Unit tests for the SSLClientCertificateRequest class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SSLClientCertificateRequestTest {
    @Mock private KeyChainCertSelectionWrapper mKeyChainMock;
    @Mock private KeyChainAliasCallback mCallbackMock;
    @Mock private CertSelectionFailureDialog mFailureDialogMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSelectCertActivityNotFound() {
        doThrow(new ActivityNotFoundException()).when(mKeyChainMock).choosePrivateKeyAlias();

        SSLClientCertificateRequest.maybeShowCertSelection(
                mKeyChainMock, mCallbackMock, mFailureDialogMock);

        verify(mKeyChainMock).choosePrivateKeyAlias();
        verify(mCallbackMock).alias(null);
        verify(mFailureDialogMock).show();
    }

    @Test
    public void testSelectCertActivityFound() {
        SSLClientCertificateRequest.maybeShowCertSelection(
                mKeyChainMock, mCallbackMock, mFailureDialogMock);

        verify(mKeyChainMock).choosePrivateKeyAlias();
        verifyNoMoreInteractions(mFailureDialogMock);
    }
}
