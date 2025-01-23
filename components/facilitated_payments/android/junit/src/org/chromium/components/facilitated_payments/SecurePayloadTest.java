// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Unit tests for {@link SecurePayload}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@SmallTest
public class SecurePayloadTest {
    private static final SecureData[] SECURE_DATA = new SecureData[] {new SecureData(1, "value_1")};
    private static final byte[] ACTION_TOKEN = new byte[] {1, 2, 3};

    @Test
    public void createSecurePayload() {

        SecurePayload securePayload = SecurePayload.create(ACTION_TOKEN, SECURE_DATA);

        Assert.assertEquals(ACTION_TOKEN, securePayload.getActionToken());
        Assert.assertEquals(1, securePayload.getSecureData().size());
    }

    @Test
    public void createSecurePayload_actionTokenNull() {
        SecurePayload securePayload = SecurePayload.create(null, SECURE_DATA);

        Assert.assertNull(securePayload);
    }

    @Test
    public void createSecurePayload_secureDataNull() {
        SecurePayload securePayload = SecurePayload.create(ACTION_TOKEN, null);

        Assert.assertNull(securePayload);
    }

    @Test
    public void createSecurePayload_secureDataEmpty() {
        SecurePayload securePayload = SecurePayload.create(ACTION_TOKEN, new SecureData[0]);

        Assert.assertEquals(ACTION_TOKEN, securePayload.getActionToken());
        Assert.assertTrue(securePayload.getSecureData().isEmpty());
    }
}
