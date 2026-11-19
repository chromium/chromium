// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.credentials.exceptions.CreateCredentialCancellationException;
import androidx.credentials.exceptions.CreateCredentialInterruptedException;
import androidx.credentials.exceptions.GetCredentialCancellationException;
import androidx.credentials.exceptions.GetCredentialInterruptedException;
import androidx.credentials.exceptions.GetCredentialUnknownException;
import androidx.credentials.exceptions.NoCredentialException;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.webid.DigitalIdentityRequestStatusForMetrics;
import org.chromium.content_public.browser.webid.DigitalIdentityUiWarningLevel;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link DigitalIdentityProvider} */
@RunWith(BaseRobolectricTestRunner.class)
public class DigitalIdentityProviderUnitTest {
    private static class TestIdentityCredentialsDelegate extends IdentityCredentialsDelegate {
        boolean mCancelled;

        @Override
        public void cancel() {
            mCancelled = true;
        }
    }

    @Test
    public void testUserDeclined() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new GetCredentialCancellationException()));
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new GetCredentialInterruptedException()));
    }

    @Test
    public void testNoCredential() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_NO_CREDENTIAL,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new NoCredentialException()));
    }

    @Test
    public void testUnknownError() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_OTHER,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new GetCredentialUnknownException("message")));
    }

    @Test
    public void testCreateUserDeclined() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new CreateCredentialCancellationException()));
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new CreateCredentialInterruptedException()));
    }

    @Test
    public void testGenericException() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_OTHER,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new Exception("message")));
    }

    @Test
    public void testRequestForwardsWarningLevelToDelegate() {
        WindowAndroid window = mock(WindowAndroid.class);
        IdentityCredentialsDelegate mockDelegate = mock(IdentityCredentialsDelegate.class);
        when(mockDelegate.get(any(), anyString(), anyString(), anyInt()))
                .thenReturn(new Promise<>());

        DigitalIdentityProvider.setDelegateForTesting(mockDelegate);
        DigitalIdentityProvider provider = DigitalIdentityProvider.create(0L);

        provider.request(
                window,
                "https://rp.example",
                "{\"requests\":[]}",
                DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);
        verify(mockDelegate)
                .get(
                        window,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);

        provider.request(
                window,
                "https://rp.example",
                "{\"requests\":[]}",
                DigitalIdentityUiWarningLevel.HINT_CAUTION);
        verify(mockDelegate)
                .get(
                        window,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_CAUTION);
    }

    @Test
    public void testDestroyCancelsInFlightRequest() {
        // Verifies native `destroy()` cancels any pending Credential Manager UI on the
        // provider's delegate.
        var testDelegate = new TestIdentityCredentialsDelegate();
        DigitalIdentityProvider.setDelegateForTesting(testDelegate);
        DigitalIdentityProvider provider = DigitalIdentityProvider.create(12345);
        provider.destroy();
        assertTrue(testDelegate.mCancelled);
    }

    @Test
    public void testDestroyForwardsCancelToActivePresentationAndCreationDelegates() {
        WindowAndroid window = mock(WindowAndroid.class);
        DigitalCredentialsPresentationDelegate mockPresentationDelegate =
                mock(DigitalCredentialsPresentationDelegate.class);
        when(mockPresentationDelegate.get(any(), anyString(), anyString(), anyInt()))
                .thenReturn(new Promise<>());

        IdentityCredentialsDelegate credentialsDelegate =
                new IdentityCredentialsDelegate() {
                    @Override
                    DigitalCredentialsPresentationDelegate createPresentationDelegate() {
                        return mockPresentationDelegate;
                    }
                };
        DigitalIdentityProvider.setDelegateForTesting(credentialsDelegate);
        DigitalIdentityProvider provider = DigitalIdentityProvider.create(111L);
        provider.request(
                window,
                "https://rp.example",
                "{\"requests\":[]}",
                DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);
        provider.destroy();
        verify(mockPresentationDelegate).cancel();

        DigitalCredentialsCreationDelegate mockCreationDelegate =
                mock(DigitalCredentialsCreationDelegate.class);
        when(mockCreationDelegate.create(any(), anyString(), anyString()))
                .thenReturn(new Promise<>());

        IdentityCredentialsDelegate creationCredentialsDelegate =
                new IdentityCredentialsDelegate() {
                    @Override
                    DigitalCredentialsCreationDelegate createCreationDelegate() {
                        return mockCreationDelegate;
                    }
                };
        DigitalIdentityProvider.setDelegateForTesting(creationCredentialsDelegate);
        DigitalIdentityProvider creationProvider = DigitalIdentityProvider.create(222L);
        creationProvider.create(window, "https://rp.example", "{\"requests\":[]}");
        creationProvider.destroy();
        verify(mockCreationDelegate).cancel();
    }
}
