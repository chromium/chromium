// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.os.Build;
import android.util.SparseArray;
import android.view.View;
import android.view.autofill.VirtualViewFillInfo;

import androidx.annotation.RequiresApi;

import java.util.Locale;

/**
 * This class is used to extract the VirtualViewFillInfo data required by {@link
 * android.view.autofill.AutofillManager#notifyVirtualViewsReady(View, SparseArray)} from the {@link
 * FormData} object sent for the cache request.
 */
class PrefillRequest {
    public static final String TAG = "PrefillRequest";
    private final FormData mForm;

    public PrefillRequest(FormData form) {
        mForm = form;
    }

    public FormData getForm() {
        return mForm;
    }

    /**
     * Parses the form currently owned by this request into a set of server predictions that the
     * Autofill framework can use.
     *
     * @return a SparseArray of VirtualViewFillInfo, null if the feature is disabled or the android
     *     version is below Android U.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public SparseArray<VirtualViewFillInfo> getPrefillHints() {
        SparseArray<VirtualViewFillInfo> virtualViewsInfo;

        // Check the comment on SparseArrayWithWorkaround class.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && AndroidAutofillFeatures.ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND.isEnabled()) {
            virtualViewsInfo = new SparseArrayWithWorkaround();
        } else {
            virtualViewsInfo = new SparseArray<>();
        }

        for (short i = 0; i < mForm.mFields.size(); ++i) {
            int virtualFieldId = FormData.toFieldVirtualId(mForm.mSessionId, i);
            // We need to send them as lower case as this is not handled
            // in the first version of Android U.
            String joinedServerPredictions =
                    mForm.mFields
                            .get(i)
                            .getServerPredictionsString()
                            .toLowerCase(Locale.getDefault());
            virtualViewsInfo.append(
                    virtualFieldId,
                    new VirtualViewFillInfo.Builder()
                            .setAutofillHints(joinedServerPredictions)
                            .build());
        }

        return virtualViewsInfo;
    }
}
