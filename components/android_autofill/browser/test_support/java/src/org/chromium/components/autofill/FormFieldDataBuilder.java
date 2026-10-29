// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.RectF;

import java.util.Collections;
import java.util.List;

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
    List<SelectOption> mOptions = Collections.emptyList();
    int mMaxLength;
    String mHeuristicType;
    String mServerType;
    String mOverallType;
    String[] mServerPredictions;
    RectF mBounds = new RectF();
    List<SelectOption> mDatalistOptions = Collections.emptyList();
    boolean mFocusable;
    boolean mVisible;
    boolean mIsAutofilled;
    RectF mBoundsInContainerViewCoordinates = new RectF();
    String mOrigin = "";

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
                        mOptions != null ? mOptions : Collections.emptyList(),
                        mMaxLength,
                        mHeuristicType,
                        mServerType,
                        mOverallType,
                        mServerPredictions,
                        mBounds.left,
                        mBounds.top,
                        mBounds.right,
                        mBounds.bottom,
                        mDatalistOptions != null ? mDatalistOptions : Collections.emptyList(),
                        mFocusable,
                        mVisible,
                        mIsAutofilled,
                        mOrigin);
        result.setBoundsInContainerViewCoordinates(mBoundsInContainerViewCoordinates);
        return result;
    }
}
