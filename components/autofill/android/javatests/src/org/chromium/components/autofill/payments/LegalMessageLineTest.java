// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.empty;
import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;

import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LegalMessageLineTest {
    private static final String EXAMPLE = "example";

    @Test
    public void testConstructor_setsText() {
        LegalMessageLine line = new LegalMessageLine(EXAMPLE);

        assertEquals(EXAMPLE, line.text);
    }

    @Test
    public void testConstructorWithLinks_setsText() {
        LegalMessageLine line = new LegalMessageLine(EXAMPLE, Collections.emptyList());

        assertEquals(EXAMPLE, line.text);
        assertThat(line.links, empty());
    }

    @Test
    public void testConstructorWithLinks_addsLinks() {
        Link link = new Link(/* start= */ 1, /* end= */ 2, /* url= */ "3");
        LegalMessageLine line = new LegalMessageLine(EXAMPLE, List.of(link));

        assertThat(line.links, contains(link));
    }

    @Test
    public void testAddLink_addsOneLinkAfterEmpty() {
        LegalMessageLine line = new LegalMessageLine(EXAMPLE);

        Link link = new Link(/* start= */ 1, /* end= */ 2, /* url= */ "3");
        line.addLink(link);
        assertThat(line.links, contains(link));
    }
}
