// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_public;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.autofill.AutofillId;

import org.chromium.base.annotations.VerifiesOnO;

/**
 * This class is used to send the server and computed view type to the autofill service.
 */
@TargetApi(Build.VERSION_CODES.O)
@VerifiesOnO
public class ViewType implements Parcelable {
    /**
     * The AutofillId of the view that types are for.
     */
    public final AutofillId mAutofillId;

    /**
     * The type from Chrome autofill server. The valid types are listed in the two
     * FieldTypeToStringPiece() functions in components/autofill/core/browser/field_types.cc. Note
     * that the list of possibly returned strings can and will change in the future.
     */
    public final String mServerType;

    /**
     * The type computed overall type. The valid types types are the same as for mServerType.
     */
    public final String mComputedType;

    public static final Parcelable.Creator<ViewType> CREATOR = new Parcelable.Creator<ViewType>() {
        @Override
        public ViewType createFromParcel(Parcel in) {
            return new ViewType(in);
        }

        @Override
        public ViewType[] newArray(int size) {
            return new ViewType[size];
        }
    };

    public ViewType(AutofillId id, String serverType, String computedType) {
        mAutofillId = id;
        mServerType = serverType;
        mComputedType = computedType;
    }

    private ViewType(Parcel in) {
        mAutofillId = AutofillId.CREATOR.createFromParcel(in);
        mServerType = in.readString();
        mComputedType = in.readString();
    }
    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        mAutofillId.writeToParcel(parcel, flags);
        parcel.writeString(mServerType);
        parcel.writeString(mComputedType);
    }
}
