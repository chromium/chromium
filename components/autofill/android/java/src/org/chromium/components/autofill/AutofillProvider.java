// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.util.SparseArray;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * This class defines interface of AutofillProvider, it doesn't use chrome's
 * autofill service or suggestion UI, instead, uses third party autofill service
 * by knowing of format structure and user's input.
 *
 * AutofillProvider handles one autofill session at time, each call of
 * queryFormFieldAutofill cancels previous session and starts a new one, the
 * calling of other methods shall associate with current session.
 *
 */
@JNINamespace("autofill")
public abstract class AutofillProvider {
    public AutofillProvider() {}

    /**
     * Invoked when container view is changed.
     *
     * @param containerView new container view.
     */
    public abstract void onContainerViewChanged(ViewGroup containerView);

    public abstract void setWebContents(WebContents webContents);

    /**
     * Invoked when autofill value is available, AutofillProvider shall fill the
     * form with the provided values.
     *
     * @param values the array of autofill values, the key is virtual id of form
     *            field.
     */
    public abstract void autofill(final SparseArray<AutofillValue> values);

    /**
     * Invoked when autofill service needs the form structure.
     *
     * @param structure see View.onProvideAutofillVirtualStructure()
     * @param flags see View.onProvideAutofillVirtualStructure()
     */
    public abstract void onProvideAutoFillVirtualStructure(ViewStructure structure, int flags);

    /**
     * @return whether query autofill suggestion.
     */
    public abstract boolean shouldQueryAutofillSuggestion();

    public abstract void queryAutofillSuggestion();

    /**
     * Invoked when filling form is need. AutofillProvider shall ask autofill
     * service for the values with which to fill the form.
     *
     * @param formData the form needs to fill.
     * @param focus the index of focus field in formData
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     */
    @CalledByNative
    protected abstract void startAutofillSession(
            FormData formData, int focus, float x, float y, float width, float height);

    /**
     * Invoked when form field's value is changed.
     *
     * @param index index of field in current form.
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     *
     */
    @CalledByNative
    protected abstract void onFormFieldDidChange(
            int index, float x, float y, float width, float height);

    /**
     * Invoked when text field is scrolled.
     *
     * @param index index of field in current form.
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     *
     */
    @CalledByNative
    protected abstract void onTextFieldDidScroll(
            int index, float x, float y, float width, float height);

    /**
     * Invoked when current form will be submitted.
     * @param submissionSource the submission source, could be any member defined in
     * SubmissionSource.java
     */
    @CalledByNative
    protected abstract void onFormSubmitted(int submissionSource);

    /**
     * Invoked when focus field changed.
     *
     * @param focusOnForm whether focus is still on form.
     * @param focusItem the index of field has focus
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     */
    @CalledByNative
    protected abstract void onFocusChanged(
            boolean focusOnForm, int focusItem, float x, float y, float width, float height);

    /**
     * Send form to renderer for filling.
     *
     * @param nativeAutofillProvider the native autofill provider.
     * @param formData the form to fill.
     */
    protected void autofill(long nativeAutofillProvider, FormData formData) {
        AutofillProviderJni.get().onAutofillAvailable(
                nativeAutofillProvider, AutofillProvider.this, formData);
    }

    /**
     * Invoked when current query need to be reset.
     */
    @CalledByNative
    protected abstract void reset();

    @CalledByNative
    protected abstract void setNativeAutofillProvider(long nativeAutofillProvider);

    @CalledByNative
    protected abstract void onDidFillAutofillFormData();

    @NativeMethods
    interface Natives {
        void onAutofillAvailable(
                long nativeAutofillProviderAndroid, AutofillProvider caller, FormData formData);
    }
}
