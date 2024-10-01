// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import androidx.annotation.Nullable;

import java.text.ParseException;
import java.util.AbstractMap;
import java.util.Map;

/**
 * A helper for parsing the optional parameters section of the {@code Content-Type} header.
 *
 * <p>See {@link https://www.rfc-editor.org/rfc/rfc9110.html#name-media-type} for more details.
 */
final class ContentTypeParametersParser {
    private static final String TOKEN_ALLOWED_SPECIAL_CHARS = "!#$%&'*+-.^_`|~";

    private final String mHeaderValue;
    private int mCurrentPosition;

    ContentTypeParametersParser(String mHeaderValue) {
        this.mHeaderValue = mHeaderValue;
        int semicolonIndex = mHeaderValue.indexOf(';');
        mCurrentPosition = semicolonIndex != -1 ? semicolonIndex + 1 : mHeaderValue.length();
    }

    @Nullable
    Map.Entry<String, String> getNextParameter() throws ContentTypeParametersParserException {
        int startPos = mCurrentPosition;
        optionallySkipWhitespace();
        String parameterName = getNextToken();
        if (currentChar() != '=') {
            throw new ContentTypeParametersParserException(
                    "Invalid parameter format: expected = at "
                            + mCurrentPosition
                            + ": ["
                            + mHeaderValue
                            + "]",
                    mCurrentPosition);
        }

        advance();

        String parameterValue;
        if (currentChar() == '"') {
            parameterValue = getNextQuotedString();
        } else {
            parameterValue = getNextToken();
        }

        optionallySkipWhitespace();

        if (hasMore()) {
            if (currentChar() != ';') {
                throw new ContentTypeParametersParserException(
                        "Invalid parameter format: expected ; at "
                                + mCurrentPosition
                                + ": ["
                                + mHeaderValue
                                + "]",
                        mCurrentPosition);
            }

            advance();
        }
        return new AbstractMap.SimpleEntry<>(parameterName, parameterValue);
    }

    private String getNextQuotedString() throws ContentTypeParametersParserException {
        int start = mCurrentPosition;
        if (currentChar() != '"') {
            throw new ContentTypeParametersParserException(
                    "Not a quoted string: expected \" at "
                            + mCurrentPosition
                            + ": ["
                            + mHeaderValue
                            + "]",
                    mCurrentPosition);
        }
        advance();

        StringBuilder sb = new StringBuilder();

        boolean escapeNext = false;

        while (true) {
            if (!hasMore()) {
                throw new ContentTypeParametersParserException(
                        "Unterminated quoted string at " + start + ": [" + mHeaderValue + "]",
                        start);
            }

            if (escapeNext) {
                if (!isQuotedPairChar(currentChar())) {
                    throw new ContentTypeParametersParserException(
                            "Invalid character at " + mCurrentPosition + ": [" + mHeaderValue + "]",
                            mCurrentPosition);
                }
                escapeNext = false;
                sb.append(currentChar());
                advance();
            } else if (currentChar() == '"') {
                advance();
                return sb.toString();
            } else if (currentChar() == '\\') {
                escapeNext = true;
                advance();
            } else {
                if (!isQdtextChar(currentChar())) {
                    throw new ContentTypeParametersParserException(
                            "Invalid character at " + mCurrentPosition + ": [" + mHeaderValue + "]",
                            mCurrentPosition);
                }
                sb.append(currentChar());
                advance();
            }
        }
    }

    private String getNextToken() throws ContentTypeParametersParserException {
        int start = mCurrentPosition;
        while (hasMore() && isTokenCharacter(currentChar())) {
            advance();
        }
        if (start == mCurrentPosition) {
            throw new ContentTypeParametersParserException(
                    "Token not found at position " + start + ": [" + mHeaderValue + "]", start);
        }
        return mHeaderValue.substring(start, mCurrentPosition);
    }

    boolean hasMore() {
        return mCurrentPosition < mHeaderValue.length();
    }

    private char currentChar() throws ContentTypeParametersParserException {
        if (!hasMore()) {
            throw new ContentTypeParametersParserException(
                    "End of header reached", mCurrentPosition);
        }
        return mHeaderValue.charAt(mCurrentPosition);
    }

    private void advance() throws ContentTypeParametersParserException {
        if (!hasMore()) {
            throw new ContentTypeParametersParserException(
                    "End of header reached", mCurrentPosition);
        }
        mCurrentPosition++;
    }

    private void optionallySkipWhitespace() throws ContentTypeParametersParserException {
        while (hasMore() && isWhitespace(currentChar())) {
            advance();
        }
    }

    private static boolean isQdtextChar(char c) {
        return c != '\\' && c != '"' && isQuotedPairChar(c);
    }

    private static boolean isQuotedPairChar(char c) {
        return isWhitespace(c) || ('!' <= c && c <= (char) 255 && c != (char) 0x7F);
    }

    private static boolean isTokenCharacter(char ch) {
        return isAscii(ch)
                && (Character.isLetterOrDigit(ch) || TOKEN_ALLOWED_SPECIAL_CHARS.indexOf(ch) != -1);
    }

    private static boolean isAscii(char ch) {
        return ch <= 127;
    }

    private static boolean isWhitespace(char c) {
        return c == '\t' || c == ' ';
    }

    static class ContentTypeParametersParserException extends ParseException {
        ContentTypeParametersParserException(String reason, int offset) {
            super(reason, offset);
        }
    }
}
