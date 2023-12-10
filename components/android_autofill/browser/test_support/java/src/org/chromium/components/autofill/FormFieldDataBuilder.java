// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.RectF;

/** Helper class to simplify {@link FormFieldData} creation. */
public class FormFieldDataBuilder {
    String mName;
    String mLabel;
    String mValue;
    String mAutocompleteAttr;
    boolean mShouldAutocomplete;
    String mPlaceholder;
    String mType;
    String mId;
    String[] mOptionValues;
    String[] mOptionContents;
    boolean mIsCheckField;
    boolean mIsChecked;
    int mMaxLength;
    String mHeuristicType;
    String mServerType;
    String mComputedType;
    String[] mServerPredictions;
    RectF mBounds = new RectF();
    String[] mDatalistValues;
    String[] mDatalistLabels;
    boolean mVisible;
    boolean mIsAutofilled;
    RectF mBoundsInContainerViewCoordinates = new RectF();

    public FormFieldData build() {
        FormFieldData result =
                FormFieldData.createFormFieldData(
                        mName,
                        mLabel,
                        mValue,
                        mAutocompleteAttr,
                        mShouldAutocomplete,
                        mPlaceholder,
                        mType,
                        mId,
                        mOptionValues,
                        mOptionContents,
                        mIsCheckField,
                        mIsChecked,
                        mMaxLength,
                        mHeuristicType,
                        mServerType,
                        mComputedType,
                        mServerPredictions,
                        mBounds.left,
                        mBounds.top,
                        mBounds.right,
                        mBounds.bottom,
                        mDatalistValues,
                        mDatalistLabels,
                        mVisible,
                        mIsAutofilled);
        result.setBoundsInContainerViewCoordinates(mBoundsInContainerViewCoordinates);
        return result;
    }
}
