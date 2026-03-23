// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertEquals;

import androidx.credentials.exceptions.CreateCredentialCancellationException;
import androidx.credentials.exceptions.CreateCredentialInterruptedException;
import androidx.credentials.exceptions.GetCredentialCancellationException;
import androidx.credentials.exceptions.GetCredentialInterruptedException;
import androidx.credentials.exceptions.GetCredentialUnknownException;
import androidx.credentials.exceptions.NoCredentialException;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.webid.DigitalIdentityRequestStatusForMetrics;

/** Tests for {@link DigitalIdentityProvider} */
@RunWith(BaseRobolectricTestRunner.class)
public class DigitalIdentityProviderUnitTest {
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
}
