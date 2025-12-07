// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertEquals;

import android.credentials.GetCredentialException;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.webid.DigitalIdentityRequestStatusForMetrics;

/** Tests for {@link DigitalIdentityProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 34)
public class DigitalIdentityProviderUnitTest {
    @Test
    public void testUserDeclined() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new GetCredentialException(
                                GetCredentialException.TYPE_USER_CANCELED, "message")));
    }

    @Test
    public void testNoCredential() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_NO_CREDENTIAL,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new GetCredentialException(
                                GetCredentialException.TYPE_NO_CREDENTIAL, "message")));
    }

    @Test
    public void testUnknownError() {
        assertEquals(
                DigitalIdentityRequestStatusForMetrics.ERROR_OTHER,
                DigitalIdentityProvider.computeStatusForMetricsFromException(
                        new GetCredentialException(
                                GetCredentialException.TYPE_UNKNOWN, "message")));
    }
}
