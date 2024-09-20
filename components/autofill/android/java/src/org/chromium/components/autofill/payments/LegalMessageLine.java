// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Legal message line with links to show in the autofill ui. */
@JNINamespace("autofill")
public class LegalMessageLine {
    /** A link in the legal message line. */
    public static class Link {
        /** The starting inclusive index of the link position in the text. */
        public int start;

        /** The ending exclusive index of the link position in the text. */
        public int end;

        /** The URL of the link. */
        public String url;

        /**
         * Creates a new instance of the link.
         *
         * @param start The starting inclusive index of the link position in the text.
         * @param end The ending exclusive index of the link position in the text.
         * @param url The URL of the link.
         */
        @CalledByNative("Link")
        public Link(int start, int end, String url) {
            this.start = start;
            this.end = end;
            this.url = url;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;

            Link link = (Link) o;
            return start == link.start && end == link.end && url.equals(link.url);
        }

        @Override
        public int hashCode() {
            return Objects.hash(start, end, url);
        }
    }

    /** The plain text legal message line. */
    public String text;

    /** A collection of links in the legal message line. */
    public final List<Link> links = new ArrayList<Link>();

    /**
     * Creates a new instance of the legal message line.
     *
     * @param text The plain text legal message.
     */
    @CalledByNative
    public LegalMessageLine(@JniType("std::u16string") String text) {
        this.text = text;
    }

    /**
     * Creates a new instance of the legal message line with text and links.
     * @param text The plain text legal message.
     * @param links List of {@link Link} objects representing the links.
     */
    @VisibleForTesting
    public LegalMessageLine(String text, List<Link> links) {
        this.text = text;
        links.forEach(this::addLink);
    }

    /**
     * Adds a link to this legal message
     *
     * @param link The link to be added.
     */
    @CalledByNative
    /*package*/ void addLink(Link link) {
        links.add(link);
    }

    /**
     * Adds a link to this legal message.
     *
     * @param start The starting inclusive index of the link position in the text.
     * @param end The ending exclusive index of the link position in the text.
     * @param url The URL of the link.
     */
    @CalledByNative
    private void addLink(int start, int end, @JniType("std::string") String url) {
        links.add(new Link(start, end, url));
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        LegalMessageLine that = (LegalMessageLine) o;
        return text.equals(that.text) && links.equals(that.links);
    }

    @Override
    public int hashCode() {
        return Objects.hash(text, links);
    }
}
