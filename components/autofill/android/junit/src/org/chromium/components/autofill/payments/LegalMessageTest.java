// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link LegalMessage}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LegalMessageTest {
    @Test
    public void testLegalMessageConstructor_setsFieldsCorrectly() {
        LegalMessageLine line = new LegalMessageLine("Legal text");
        AtomicReference<String> clickedUrl = new AtomicReference<>();

        LegalMessage legalMessage = new LegalMessage(List.of(line), clickedUrl::set);

        assertThat(legalMessage.mLines, equalTo(List.of(line)));
        legalMessage.mLink.accept("https://example.com");
        assertThat(clickedUrl.get(), equalTo("https://example.com"));
    }

    @Test
    public void testLegalMessageConstructor_emptyLines() {
        AtomicReference<String> clickedUrl = new AtomicReference<>();
        LegalMessage legalMessage = new LegalMessage(clickedUrl::set);

        assertThat(legalMessage.mLines, equalTo(Collections.emptyList()));
    }

    @Test
    public void testLegalMessageConstructor_nullParametersThrow() {
        assertThrows(NullPointerException.class, () -> new LegalMessage(null, url -> {}));
        assertThrows(
                NullPointerException.class, () -> new LegalMessage(Collections.emptyList(), null));
        assertThrows(NullPointerException.class, () -> new LegalMessage(null));
    }
}
