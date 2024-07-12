// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.Rect;
import android.util.SparseArray;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import org.chromium.base.Log;
import org.chromium.components.autofill_public.ViewType;

import java.util.ArrayList;

/**
 * A class to translate {@link FormData} into the ViewStructure used by Android's Autofill
 * framework. It uses the {@link FormData#fillViewStructure(ViewStructure)} to translate the
 * FormData object into a ViewStructure.
 *
 * <p>The key method of this class is: - {@link #autofill}: Verifies that the autofill request by
 * the framework is valid.
 */
public class AutofillRequest {
    /** A simple class representing the field that is currently focused by the user. */
    public static class FocusField {
        public final short fieldIndex;
        public final Rect absBound;

        public FocusField(short fieldIndex, Rect absBound) {
            this.fieldIndex = fieldIndex;
            this.absBound = absBound;
        }
    }

    private static final String TAG = "AutofillRequest";

    private FormData mFormData;
    private FocusField mFocusField;
    private AutofillHintsService mAutofillHintsService;

    /**
     * @param formData the form of the AutofillRequest.
     * @param focus the currently focused field.
     * @param hasServerPrediction whether the server type of formData is valid.
     */
    public AutofillRequest(FormData formData, FocusField focus, boolean hasServerPrediction) {
        mFormData = formData;
        mFocusField = focus;
        // Don't need to create binder object if server prediction is already available.
        if (!hasServerPrediction) mAutofillHintsService = new AutofillHintsService();
    }

    /**
     * Verifies that the values of this autofill request from the framework have virtual ids that
     * match the session id and the ids of existing form fields of the selected form. If they do, it
     * updates the underlying FormFieldData objects to contain the new values, which are then used
     * by the native code to fill the form.
     *
     * @param values the autofill request by the Android Autofill framework
     * @return whether the autofill request is valid, i.e. whether the virtual ids contained in it
     *     correspond to an ongoing session with existing form fields.
     */
    public boolean autofill(final SparseArray<AutofillValue> values) {
        int filledCount = 0;
        for (int i = 0; i < values.size(); ++i) {
            int id = values.keyAt(i);
            if (toSessionId(id) != mFormData.mSessionId) continue;
            AutofillValue value = values.get(id);
            if (value == null) continue;
            short index = toIndex(id);
            if (index < 0 || index >= mFormData.mFields.size()) return false;
            FormFieldData field = mFormData.mFields.get(index);
            if (field == null) return false;
            try {
                switch (field.getControlType()) {
                    case FormFieldData.ControlType.LIST:
                        int j = value.getListValue();
                        if (j < 0 || j >= field.mOptionValues.length) continue;
                        field.setAutofillValue(field.mOptionValues[j]);
                        break;
                    case FormFieldData.ControlType.TOGGLE:
                        field.setChecked(value.getToggleValue());
                        break;
                    case FormFieldData.ControlType.TEXT:
                    case FormFieldData.ControlType.DATALIST:
                        field.setAutofillValue((String) value.getTextValue());
                        break;
                    default:
                        break;
                }
            } catch (IllegalStateException e) {
                // Refer to crbug.com/1080580 .
                Log.e(TAG, "The given AutofillValue wasn't expected, abort autofill.", e);
                return false;
            }
            filledCount++;
        }
        return filledCount != 0;
    }

    public void setFocusField(FocusField focusField) {
        mFocusField = focusField;
    }

    public FocusField getFocusField() {
        return mFocusField;
    }

    public int getFieldCount() {
        return mFormData.mFields.size();
    }

    public AutofillValue getFieldNewValue(int index) {
        FormFieldData field = mFormData.mFields.get(index);
        if (field == null) return null;
        switch (field.getControlType()) {
            case FormFieldData.ControlType.LIST:
                int i = FormData.findIndex(field.mOptionValues, field.getValue());
                if (i == -1) return null;
                return AutofillValue.forList(i);
            case FormFieldData.ControlType.TOGGLE:
                return AutofillValue.forToggle(field.isChecked());
            case FormFieldData.ControlType.TEXT:
            case FormFieldData.ControlType.DATALIST:
                return AutofillValue.forText(field.getValue());
            default:
                return null;
        }
    }

    public int getFieldVirtualId(short fieldIndex) {
        return FormData.toFieldVirtualId(mFormData.mSessionId, fieldIndex);
    }

    public FormData getForm() {
        return mFormData;
    }

    public FormFieldData getField(short fieldIndex) {
        return mFormData.mFields.get(fieldIndex);
    }

    private static int toSessionId(int fieldVirtualId) {
        return (fieldVirtualId & 0xffff0000) >> 16;
    }

    private static short toIndex(int fieldVirtualId) {
        return (short) (fieldVirtualId & 0xffff);
    }

    public AutofillHintsService getAutofillHintsService() {
        return mAutofillHintsService;
    }

    public void onServerPredictionsAvailable() {
        if (mAutofillHintsService == null) return;
        ArrayList<ViewType> viewTypes = new ArrayList<ViewType>();
        for (FormFieldData field : mFormData.mFields) {
            viewTypes.add(
                    new ViewType(
                            field.getAutofillId(),
                            field.getServerType(),
                            field.getComputedType(),
                            field.getServerPredictions()));
            }
        mAutofillHintsService.onViewTypeAvailable(viewTypes);
    }
}
