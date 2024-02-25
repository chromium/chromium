// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.Objects;

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
    }

    @Test
    public void testConstructorWithLinks_addsLinks() {
        LegalMessageLine line =
                new LegalMessageLine(
                        EXAMPLE,
                        Arrays.asList(new Link(/* start= */ 1, /* end= */ 2, /* url= */ "3")));

        assertThat(line.links, contains(equalToLink(/* start= */ 1, /* end= */ 2, /* url= */ "3")));
    }

    @Test
    public void testConstructor_createsWithNoLinks() {
        LegalMessageLine line = new LegalMessageLine(EXAMPLE);

        assertThat(line.links, empty());
    }

    @Test
    public void testAddLink_addsOneLinkAfterEmpty() {
        LegalMessageLine line = new LegalMessageLine(EXAMPLE);

        line.addLink(new LegalMessageLine.Link(1, 2, "3"));
        assertThat(line.links, contains(equalToLink(1, 2, "3")));
    }

    @Test
    public void testAddToList_createsNewList() {
        LinkedList<LegalMessageLine> list =
                LegalMessageLine.addToList_createListIfNull(null, "example");

        assertThat(list, notNullValue());
        assertThat(list, contains(equalToLegalMessageLine("example", /* link= */ null)));
    }

    @Test
    public void testAddToList_addsToExistingList() {
        LinkedList<LegalMessageLine> list = new LinkedList<>();
        list.add(new LegalMessageLine("first"));

        list = LegalMessageLine.addToList_createListIfNull(list, "second");

        assertThat(list, notNullValue());
        assertThat(
                list,
                contains(
                        equalToLegalMessageLine("first", /* link= */ null),
                        equalToLegalMessageLine("second", /* link= */ null)));
    }

    @Test
    public void testAddLinkToLastInList() {
        LinkedList<LegalMessageLine> list = new LinkedList<>();
        list.add(new LegalMessageLine("example"));

        LegalMessageLine.addLinkToLastInList(list, 0, 1, "https://example.test");

        assertThat(list, notNullValue());
        assertThat(
                list,
                contains(
                        equalToLegalMessageLine(
                                "example", new Link(0, 1, "https://example.test"))));
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

    private static Matcher<LegalMessageLine> equalToLegalMessageLine(String text, Link link) {
        return new TypeSafeMatcher<LegalMessageLine>() {
            @Override
            protected boolean matchesSafely(LegalMessageLine line) {
                if (!Objects.equals(line.text, text)) return false;

                if (link == null) return line.links.isEmpty();

                if (line.links.size() != 1) return false;

                return link.start == line.links.get(0).start
                        && link.end == line.links.get(0).end
                        && Objects.equals(link.url, line.links.get(0).url);
            }

            @Override
            public void describeTo(Description description) {
                if (link == null) {
                    description.appendText(String.format("LegalMessageLine(%s)", text));
                } else {
                    description.appendText(
                            String.format(
                                    "LegalMessageLine(%s, Link(%s, %s, %s))",
                                    text, link.start, link.end, link.url));
                }
            }
        };
    }
}
