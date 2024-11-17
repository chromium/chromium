// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.RectF;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.List;

/**
 * The wrapper class of the native autofill::FormDataAndroid.
 *
 * <p>{@link #fillViewStructure(ViewStructure)} is used by other classes (i.e {@link
 * AutofillRequest} to translate the FormData object into a ViewStructure.
 */
@JNINamespace("autofill")
public class FormData {
    public final int mSessionId;
    public final String mName;
    public final String mHost;
    public final List<FormFieldData> mFields;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    static FormData createFormData(
            int sessionId,
            @JniType("std::u16string") String name,
            @JniType("std::string") String origin,
            @JniType("std::vector") List<FormFieldData> fields) {
        return new FormData(sessionId, name, origin, fields);
    }

    public FormData(int sessionId, String name, String host, List<FormFieldData> fields) {
        mSessionId = sessionId;
        mName = name;
        mHost = host;
        mFields = fields;
    }

    /**
     * Translates the current form into a ViewStructure processed by Android's Autofill framework.
     *
     * @param structure out parameter, the structure passed to the framework.
     * @param focusFieldIndex the index of the field that is currently focused. -1 if unknown.
     */
    public void fillViewStructure(ViewStructure structure, short focusFieldIndex) {
        structure.setWebDomain(mHost);
        structure.setHtmlInfo(
                structure.newHtmlInfoBuilder("form").addAttribute("name", mName).build());
        int index = structure.addChildCount(mFields.size());
        short fieldIndex = 0;
        for (FormFieldData field : mFields) {
            ViewStructure child = structure.newChild(index++);
            if (focusFieldIndex == fieldIndex) {
                child.setFocused(true);
            }
            int virtualId = toFieldVirtualId(mSessionId, fieldIndex++);
            child.setAutofillId(structure.getAutofillId(), virtualId);
            field.setAutofillId(child.getAutofillId());
            if (field.mAutocompleteAttr != null && !field.mAutocompleteAttr.isEmpty()) {
                child.setAutofillHints(field.mAutocompleteAttr.split(" +"));
            }
            child.setHint(field.mPlaceholder);

            RectF bounds = field.getBoundsInContainerViewCoordinates();
            // Field has no scroll.
            child.setDimens(
                    (int) bounds.left,
                    (int) bounds.top,
                    /* scrollX= */ 0,
                    /* scrollY= */ 0,
                    (int) bounds.width(),
                    (int) bounds.height());
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

    static int toFieldVirtualId(int sessionId, short index) {
        return (sessionId << 16) | index;
    }

    static int findIndex(String[] values, String value) {
        if (values != null && value != null) {
            for (int i = 0; i < values.length; i++) {
                if (value.equals(values[i])) return i;
            }
        }
        return -1;
    }
}
