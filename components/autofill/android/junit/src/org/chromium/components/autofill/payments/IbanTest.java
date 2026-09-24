// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link Iban} equals and hashCode consistency. */
@RunWith(BaseRobolectricTestRunner.class)
public class IbanTest {
    private static final String GUID = "123e4567-e89b-12d3-a456-426614174000";
    private static final long INSTRUMENT_ID = 123456789L;
    private static final String LABEL = "CH •••8009";
    private static final String NICKNAME = "My IBAN";
    private static final String VALUE = "CH5604835012345678009";

    @Test
    public void testEqualsAndHashCode_localIban_equal() {
        Iban iban1 = Iban.createLocal(GUID, LABEL, NICKNAME, VALUE);
        Iban iban2 = Iban.createLocal(GUID, LABEL, NICKNAME, VALUE);

        assertThat(iban1).isEqualTo(iban2);
        assertThat(iban1.hashCode()).isEqualTo(iban2.hashCode());
    }

    @Test
    public void testEqualsAndHashCode_serverIban_equal() {
        Iban iban1 = Iban.createServer(INSTRUMENT_ID, LABEL, NICKNAME, "");
        Iban iban2 = Iban.createServer(INSTRUMENT_ID, LABEL, NICKNAME, "");

        assertThat(iban1).isEqualTo(iban2);
        assertThat(iban1.hashCode()).isEqualTo(iban2.hashCode());
    }

    @Test
    public void testEquals_localAndServerIbanWithSameFields_notEqual() {
        Iban local = Iban.createLocal(GUID, LABEL, NICKNAME, "");
        Iban server = Iban.createServer(INSTRUMENT_ID, LABEL, NICKNAME, "");

        assertThat(local).isNotEqualTo(server);
    }
}
