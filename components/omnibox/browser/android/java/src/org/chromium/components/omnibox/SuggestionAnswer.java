// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.annotations.CalledByNative;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Contains the data needed to renderer a answer in the Omnibox suggestions.
 */
public class SuggestionAnswer {
    @AnswerType
    private final int mType;
    private final ImageLine mFirstLine;
    private final ImageLine mSecondLine;

    @VisibleForTesting
    public SuggestionAnswer(@AnswerType int type, ImageLine firstLine, ImageLine secondLine) {
        mType = type;
        mFirstLine = firstLine;
        mSecondLine = secondLine;
    }

    /** Return the type of Answer being shown. */
    @AnswerType
    public int getType() {
        return mType;
    }

    /** Returns the first of the two required image lines. */
    public ImageLine getFirstLine() {
        return mFirstLine;
    }

    /** Returns the second of the two required image lines. */
    public ImageLine getSecondLine() {
        return mSecondLine;
    }

    @Override
    public int hashCode() {
        // TODO(tedchoc): Replace usage of Arrays.hashCode with ObjectsCompat.hash once the support
        //                library is rolled past 27.1.
        return Arrays.hashCode(new Object[] {mType, mFirstLine, mSecondLine});
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof SuggestionAnswer)) return false;
        SuggestionAnswer other = (SuggestionAnswer) obj;
        return mType == other.mType && ObjectsCompat.equals(mFirstLine, other.mFirstLine)
                && ObjectsCompat.equals(mSecondLine, other.mSecondLine);
    }

    /**
     * Represents a single line of an answer, containing any number of typed text fields and an
     * optional image.
     */
    public static class ImageLine {
        private final List<TextField> mTextFields;
        private final TextField mAdditionalText;
        private final TextField mStatusText;
        private final String mImage;

        @VisibleForTesting
        public ImageLine(List<TextField> textFields, TextField additionalText, TextField statusText,
                String imageUrl) {
            mTextFields = textFields;
            mAdditionalText = additionalText;
            mStatusText = statusText;
            mImage = imageUrl;
        }

        /**
         * Return an unnamed list of text fields.  These represent the main content of the line.
         */
        public List<TextField> getTextFields() {
            return mTextFields;
        }

        /**
         * Returns true if the line contains an "additional text" field.
         */
        public boolean hasAdditionalText() {
            return mAdditionalText != null;
        }

        /**
         * Return the "additional text" field.
         */
        public TextField getAdditionalText() {
            return mAdditionalText;
        }

        /**
         * Returns true if the line contains an "status text" field.
         */
        public boolean hasStatusText() {
            return mStatusText != null;
        }

        /**
         * Return the "status text" field.
         */
        public TextField getStatusText() {
            return mStatusText;
        }

        /**
         * Returns true if the line contains an image.
         */
        public boolean hasImage() {
            return mImage != null;
        }

        /**
         * Return the optional image (URL or base64-encoded image data).
         */
        public String getImage() {
            return mImage;
        }

        @Override
        public int hashCode() {
            return Arrays.deepHashCode(
                    new Object[] {mTextFields.toArray(), mAdditionalText, mStatusText, mImage});
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof ImageLine)) return false;
            ImageLine other = (ImageLine) obj;

            if (mTextFields.size() != other.mTextFields.size()) return false;
            for (int i = 0; i < mTextFields.size(); i++) {
                if (!ObjectsCompat.equals(mTextFields.get(i), other.mTextFields.get(i))) {
                    return false;
                }
            }
            return TextUtils.equals(mImage, other.mImage)
                    && ObjectsCompat.equals(mAdditionalText, other.mAdditionalText)
                    && ObjectsCompat.equals(mStatusText, other.mStatusText);
        }
    }

    /**
     * Represents one text field of an answer, containing a integer type and a string.
     */
    public static class TextField {
        @AnswerTextType
        private final int mType;
        private final String mText;
        @AnswerTextStyle
        private final int mStyle;
        private final int mNumLines;

        @VisibleForTesting
        public TextField(
                @AnswerTextType int type, String text, @AnswerTextStyle int style, int numLines) {
            mType = type;
            mText = text;
            mStyle = style;
            mNumLines = numLines;
        }

        @AnswerTextType
        public int getType() {
            return mType;
        }

        public String getText() {
            return mText;
        }

        @AnswerTextStyle
        public int getStyle() {
            return mStyle;
        }

        public boolean hasNumLines() {
            return mNumLines != -1;
        }

        public int getNumLines() {
            return mNumLines;
        }

        @Override
        public int hashCode() {
            return Arrays.hashCode(new Object[] {mType, mText, mStyle, mNumLines});
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof TextField)) return false;
            TextField other = (TextField) obj;
            return mType == other.mType && TextUtils.equals(mText, other.mText)
                    && mStyle == other.mStyle && mNumLines == other.mNumLines;
        }
    }

    @CalledByNative
    private static SuggestionAnswer createSuggestionAnswer(
            @AnswerType int type, ImageLine firstLine, ImageLine secondLine) {
        return new SuggestionAnswer(type, firstLine, secondLine);
    }

    @CalledByNative
    private static ImageLine createImageLine(List<TextField> fields, TextField additionalText,
            TextField statusText, String imageUrl) {
        return new ImageLine(fields, additionalText, statusText, imageUrl);
    }

    @CalledByNative
    private static List<TextField> createTextFieldList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addTextFieldToList(List<TextField> list, TextField field) {
        list.add(field);
    }

    @CalledByNative
    private static TextField createTextField(
            @AnswerTextType int type, String text, @AnswerTextStyle int style, int numLines) {
        return new TextField(type, text, style, numLines);
    }
}
