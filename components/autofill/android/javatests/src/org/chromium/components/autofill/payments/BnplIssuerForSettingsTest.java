// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.R;

/** Unit tests for {@link BnplIssuerForSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BnplIssuerForSettingsTest {
    private static final long ISSUER_INSTRUMENT_ID = 100;
    private static final String ISSUER_NAME = "Affirm";

    @Test
    public void testConstructor() throws Exception {
        BnplIssuerForSettings bnplIssuerForSettings =
                new BnplIssuerForSettings(
                        /* iconId= */ R.drawable.bnpl_icon_generic,
                        /* instrumentId= */ ISSUER_INSTRUMENT_ID,
                        /* displayName= */ ISSUER_NAME);

        assertThat(bnplIssuerForSettings.getIconId()).isEqualTo(R.drawable.bnpl_icon_generic);
        assertThat(bnplIssuerForSettings.getInstrumentId()).isEqualTo(ISSUER_INSTRUMENT_ID);
        assertThat(bnplIssuerForSettings.getDisplayName()).isEqualTo(ISSUER_NAME);
    }
}
