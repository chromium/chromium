// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.autofill_public.ViewType;

import java.util.ArrayList;

/**
 * A class to translate {@link FormData} into the ViewStructure used by Android's
 * Autofill framework.
 *
 * The key methods of this class are:
 * - {@link #fillViewStructure}: Translates the FormData in this object into a ViewStructure.
 * - {@link #autofill}: Verifies that the autofill request by the framework is valid.
 */
@RequiresApi(Build.VERSION_CODES.O)
public class AutofillRequest {
    /**
     * A simple class representing the field that is currently focused by the user.
     */
    public static class FocusField {
        public final short fieldIndex;
        public final Rect absBound;

        public FocusField(short fieldIndex, Rect absBound) {
            this.fieldIndex = fieldIndex;
            this.absBound = absBound;
        }
    }

    private static final String TAG = "AutofillRequest";
    // The id cannot be 0 in Android.
    private static final int INIT_ID = 1;
    // Every node must have an Autofill id. We (arbitrarily, but consistently) choose the
    // maximum value for the form node.
    private static final short FORM_NODE_ID = Short.MAX_VALUE;
    private static int sSessionId = INIT_ID;
    public final int sessionId;
    private FormData mFormData;
    private FocusField mFocusField;
    private AutofillHintsService mAutofillHintsService;

    /**
     * @param formData the form of the AutofillRequest.
     * @param focus the currently focused field.
     * @param hasServerPrediction whether the server type of formData is valid.
     */
    public AutofillRequest(FormData formData, FocusField focus, boolean hasServerPrediction) {
        sessionId = getNextClientId();
        mFormData = formData;
        mFocusField = focus;
        // Don't need to create binder object if server prediction is already available.
        if (!hasServerPrediction) mAutofillHintsService = new AutofillHintsService();
    }

    /**
     * Translates mFormData into a ViewStructure processed by Android's Autofill framework.
     *
     * @param structure out parameter, the structure passed to the framework.
     */
    public void fillViewStructure(ViewStructure structure) {
        // If the experiment is on, then the root node's children correspond to forms and the actual
        // fields are leaf nodes with depth 2.
        if (AndroidAutofillFeatures.ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER
                        .isEnabled()) {
            ViewStructure rootStructure = structure;
            structure = rootStructure.newChild(rootStructure.addChildCount(1));
            structure.setAutofillId(
                    rootStructure.getAutofillId(), toVirtualId(sessionId, FORM_NODE_ID));
        }
        structure.setWebDomain(mFormData.mHost);
        structure.setHtmlInfo(
                structure.newHtmlInfoBuilder("form").addAttribute("name", mFormData.mName).build());
        int index = structure.addChildCount(mFormData.mFields.size());
        short fieldIndex = 0;
        for (FormFieldData field : mFormData.mFields) {
            ViewStructure child = structure.newChild(index++);
            int virtualId = toVirtualId(sessionId, fieldIndex++);
            child.setAutofillId(structure.getAutofillId(), virtualId);
            field.setAutofillId(child.getAutofillId());
            if (field.mAutocompleteAttr != null && !field.mAutocompleteAttr.isEmpty()) {
                child.setAutofillHints(field.mAutocompleteAttr.split(" +"));
            }
            child.setHint(field.mPlaceholder);

            RectF bounds = field.getBoundsInContainerViewCoordinates();
            // Field has no scroll.
            child.setDimens((int) bounds.left, (int) bounds.top, 0 /* scrollX*/, 0 /* scrollY */,
                    (int) bounds.width(), (int) bounds.height());
            child.setVisibility(field.getVisible() ? View.VISIBLE : View.INVISIBLE);

            ViewStructure.HtmlInfo.Builder builder =
                    child.newHtmlInfoBuilder("input")
                            .addAttribute("name", field.mName)
                            .addAttribute("type", field.mType)
                            .addAttribute("label", field.mLabel)
                            .addAttribute("ua-autofill-hints", field.mHeuristicType)
                            .addAttribute("id", field.mId);
            builder.addAttribute("crowdsourcing-autofill-hints", field.getServerType());
            builder.addAttribute("computed-autofill-hints", field.getComputedType());
            // Compose multiple predictions to a string separated by ','.
            String[] predictions = field.getServerPredictions();
            if (predictions != null && predictions.length > 0) {
                builder.addAttribute(
                        "crowdsourcing-predictions-autofill-hints", String.join(",", predictions));
            }
            switch (field.getControlType()) {
                case FormFieldData.ControlType.LIST:
                    child.setAutofillType(View.AUTOFILL_TYPE_LIST);
                    child.setAutofillOptions(field.mOptionContents);
                    int i = findIndex(field.mOptionValues, field.getValue());
                    if (i != -1) {
                        child.setAutofillValue(AutofillValue.forList(i));
                    }
                    break;
                case FormFieldData.ControlType.TOGGLE:
                    child.setAutofillType(View.AUTOFILL_TYPE_TOGGLE);
                    child.setAutofillValue(AutofillValue.forToggle(field.isChecked()));
                    break;
                case FormFieldData.ControlType.TEXT:
                case FormFieldData.ControlType.DATALIST:
                    child.setAutofillType(View.AUTOFILL_TYPE_TEXT);
                    child.setAutofillValue(AutofillValue.forText(field.getValue()));
                    if (field.mMaxLength != 0) {
                        builder.addAttribute("maxlength", String.valueOf(field.mMaxLength));
                    }
                    if (field.getControlType() == FormFieldData.ControlType.DATALIST) {
                        child.setAutofillOptions(field.mDatalistValues);
                    }
                    break;
                default:
                    break;
            }
            child.setHtmlInfo(builder.build());
        }
    }

    /**
     * Verifies that the values of this autofill request from the framework has virtual ids
     * that match the session id and the ids of existing form fields of the selected form.
     * If they do, it updates the underlying FormFieldData objects to contain the new values,
     * which are then used by the native code to fill the form.
     *
     * @param values the autofill request by the Android Autofill framework
     * @return whether the autofill request is valid, i.e. whether the virtual ids contained
     * in it correspond to an ongoing session with existing form fields.
     */
    public boolean autofill(final SparseArray<AutofillValue> values) {
        for (int i = 0; i < values.size(); ++i) {
            int id = values.keyAt(i);
            if (toSessionId(id) != sessionId) return false;
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
        }
        return true;
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
                int i = findIndex(field.mOptionValues, field.getValue());
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

    public int getVirtualId(short index) {
        return toVirtualId(sessionId, index);
    }

    public FormData getForm() {
        return mFormData;
    }

    public FormFieldData getField(short index) {
        return mFormData.mFields.get(index);
    }

    private static int findIndex(String[] values, String value) {
        if (values != null && value != null) {
            for (int i = 0; i < values.length; i++) {
                if (value.equals(values[i])) return i;
            }
        }
        return -1;
    }

    private static int getNextClientId() {
        ThreadUtils.assertOnUiThread();
        if (sSessionId == 0xffff) sSessionId = INIT_ID;
        return sSessionId++;
    }

    private static int toSessionId(int virtualId) {
        return (virtualId & 0xffff0000) >> 16;
    }

    private static short toIndex(int virtualId) {
        return (short) (virtualId & 0xffff);
    }

    private static int toVirtualId(int clientId, short index) {
        return (clientId << 16) | index;
    }

    public AutofillHintsService getAutofillHintsService() {
        return mAutofillHintsService;
    }

    public void onQueryDone(boolean success) {
        if (mAutofillHintsService == null) return;
        if (success) {
            ArrayList<ViewType> viewTypes = new ArrayList<ViewType>();
            for (FormFieldData field : mFormData.mFields) {
                viewTypes.add(new ViewType(field.getAutofillId(), field.getServerType(),
                        field.getComputedType(), field.getServerPredictions()));
            }
            mAutofillHintsService.onViewTypeAvailable(viewTypes);
        } else {
            mAutofillHintsService.onQueryFailed();
        }
    }
}
