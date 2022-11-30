// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import java.util.ArrayList;
import java.util.List;

/**
 * Encapsulates simple wildcard pattern-matching.
 * See {@link PatternMatcher#PatternMatcher(String)} for the format description.
 */
class PatternMatcher {
    /** Encapsulates information about illegal pattern. */
    public static class IllegalPatternException extends Exception {
        private IllegalPatternException(String message, String pattern) {
            super(String.format("Illegal pattern '%s': %s", pattern, message));
        }
    }

    // The pattern is split by wildcards and stored as a list of chunks with escaping removed
    // Thus, the first chunk contains pattern characters from the beginning to the first wildcard,
    // the second chunk contains characters from the first wildcard to the second one, etc.
    private final List<String> mChunks = new ArrayList<>();

    /**
     * Constructs a matcher from the pattern string.
     *
     * Pattern wildcards and special characters:
     * Asterisk * - matches arbitrary sequence of characters (including empty sequence);
     * Backslash \ - escapes the following character, so '\*' will match literal asterisk only. May
     *     also escape characters that don't have any special meaning, so 't' and '\t' are the same
     *     patterns.
     *
     * @param pattern the pattern to match strings against.
     */
    PatternMatcher(String pattern) throws IllegalPatternException {
        // Split pattern by * wildcards and un-escape.
        boolean escape = false;
        StringBuilder currentChunk = new StringBuilder();
        for (int i = 0; i < pattern.length(); i++) {
            char patternChar = pattern.charAt(i);
            if (!escape && patternChar == '\\') {
                escape = true;
                continue;
            }
            if (!escape && patternChar == '*') {
                mChunks.add(currentChunk.toString());
                currentChunk = new StringBuilder();
                continue;
            }
            currentChunk.append(patternChar);
            escape = false;
        }
        if (escape) {
            throw new IllegalPatternException("Unmatched escape symbol at the end", pattern);
        }
        mChunks.add(currentChunk.toString());
    }

    /**
     * Checks whether the whole given string matches the pattern.
     *
     * @param string the string to match against the pattern.
     * @return whether the whole string matches the pattern.
     */
    boolean matches(String string) {
        // No wildcards, the whole string should match the pattern.
        if (mChunks.size() == 1) {
            return string.equals(mChunks.get(0));
        }
        // The first chunk should match the string head.
        String firstChunk = mChunks.get(0);
        if (!string.startsWith(firstChunk)) return false;
        // The last chunk should match the string tail.
        String lastChunk = mChunks.get(mChunks.size() - 1);
        if (!string.endsWith(lastChunk)) return false;
        // Greedy match all the rest of the chunks.
        int stringOffset = firstChunk.length();
        for (String chunk : mChunks.subList(1, mChunks.size() - 1)) {
            int offset = string.indexOf(chunk, stringOffset);
            if (offset == -1) return false;
            stringOffset = offset + chunk.length();
        }
        return stringOffset + lastChunk.length() <= string.length();
    }
}
