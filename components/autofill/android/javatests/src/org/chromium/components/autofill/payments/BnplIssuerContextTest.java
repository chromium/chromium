// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BnplIssuerContext}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BnplIssuerContextTest {
    @Test
    public void testConstructor() throws Exception {
        BnplIssuerContext bnplIssuerContext =
                new BnplIssuerContext(
                        123, "Test Id", "Test Issuer", "Test Description", false, true);

        assertThat(bnplIssuerContext.getIconId()).isEqualTo(123);
        assertThat(bnplIssuerContext.getIssuerId()).isEqualTo("Test Id");
        assertThat(bnplIssuerContext.getDisplayName()).isEqualTo("Test Issuer");
        assertThat(bnplIssuerContext.getSelectionText()).isEqualTo("Test Description");
        assertThat(bnplIssuerContext.isLinked()).isFalse();
        assertThat(bnplIssuerContext.isEligible()).isTrue();
    }
}
