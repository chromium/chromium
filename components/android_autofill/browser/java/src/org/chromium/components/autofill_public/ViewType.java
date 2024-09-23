// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_public;

import android.os.Parcel;
import android.os.Parcelable;
import android.view.autofill.AutofillId;

/**
 * This class is used to send the server and computed view type to the autofill service. The valid
 * types are listed in the FieldTypeToStringView() function in
 * components/autofill/core/browser/field_types.cc. Note that the list of possibly returned strings
 * can and will change in the future.
 */
public class ViewType implements Parcelable {
    /** The AutofillId of the view that types are for. */
    public final AutofillId mAutofillId;

    /** The type from Chrome autofill server. */
    public final String mServerType;

    /** The type computed overall type. The valid types are the same as for mServerType. */
    public final String mComputedType;

    private String[] mServerPredictions;

    public static final Parcelable.Creator<ViewType> CREATOR =
            new Parcelable.Creator<ViewType>() {
                @Override
                public ViewType createFromParcel(Parcel in) {
                    return new ViewType(in);
                }

                @Override
                public ViewType[] newArray(int size) {
                    return new ViewType[size];
                }
            };

    public ViewType(
            AutofillId id, String serverType, String computedType, String[] serverPredictions) {
        mAutofillId = id;
        mServerType = serverType;
        mComputedType = computedType;
        mServerPredictions = serverPredictions;
    }

    private ViewType(Parcel in) {
        mAutofillId = AutofillId.CREATOR.createFromParcel(in);
        mServerType = in.readString();
        mComputedType = in.readString();
        in.readStringArray(mServerPredictions);
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
        parcel.writeStringArray(mServerPredictions);
    }

    /**
     * @return the server predictions, they are in the order of the confidence. The mServerType
     * shall be used if the server predictions aren't available.
     */
    public String[] getServerPredictions() {
        return mServerPredictions;
    }
}
