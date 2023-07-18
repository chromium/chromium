// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.equalTo;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;

import java.util.Objects;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LegalMessageLineTest {
    @Test
    public void testConstructor_setsText() {
        LegalMessageLine line = new LegalMessageLine("example");

        assertThat(line.text, equalTo("example"));
    }

    @Test
    public void testConstructor_createsWithNoLinks() {
        LegalMessageLine line = new LegalMessageLine("example");

        assertThat(line.links, empty());
    }

    @Test
    public void testAddLink_addsOneLinkAfterEmpty() {
        LegalMessageLine line = new LegalMessageLine("example");

        line.addLink(new LegalMessageLine.Link(1, 2, "3"));
        assertThat(line.links, contains(equalToLink(1, 2, "3")));
    }

    private static Matcher<Link> equalToLink(int start, int end, String url) {
        return new TypeSafeMatcher<Link>() {
            @Override
            protected boolean matchesSafely(Link link) {
                return link.start == start && link.end == end && Objects.equals(link.url, url);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText(
                        String.format("LegalMessageLine.Link(%s, %s, %s)", start, end, url));
            }
        };
    }
}
