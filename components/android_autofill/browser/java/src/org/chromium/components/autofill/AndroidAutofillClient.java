// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.ComponentName;
import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.autofill.AutofillManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;

/**
 * Java counterpart to AndroidAutofillClient. This class is created by AwContents or ContentView and
 * has a weak reference from native side. Its lifetime matches the duration of the WebView or the
 * WebContents using it.
 */
@JNINamespace("android_autofill")
public class AndroidAutofillClient {
    private static final String AWG_COMPONENT_NAME =
            "com.google.android.gms/com.google.android.gms.autofill.service.AutofillService";

    private final long mNativeAndroidAutofillClient;
    private AutofillPopup mAutofillPopup;
    private Context mContext;

    @CalledByNative
    public static AndroidAutofillClient create(long nativeClient) {
        return new AndroidAutofillClient(nativeClient);
    }

    @CalledByNative
    public static boolean allowedForAutofillService() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return false;
        }
        AutofillManager manager =
                ContextUtils.getApplicationContext().getSystemService(AutofillManager.class);
        if (manager == null || !manager.isEnabled()) {
            return false;
        }
        ComponentName componentName = null;
        try {
            componentName = manager.getAutofillServiceComponentName();
        } catch (Exception e) {
        }
        return componentName != null && !AWG_COMPONENT_NAME.equals(componentName.flattenToString());
    }

    private AndroidAutofillClient(long nativeAndroidAutofillClient) {
        mNativeAndroidAutofillClient = nativeAndroidAutofillClient;
    }

    public void init(Context context) {
        mContext = context;
    }

    @CalledByNative
    private void showAutofillPopup(
            View anchorView, boolean isRtl, AutofillSuggestion[] suggestions) {

        if (mAutofillPopup == null) {
            if (ContextUtils.activityFromContext(mContext) == null) {
                AndroidAutofillClientJni.get()
                        .dismissed(mNativeAndroidAutofillClient, AndroidAutofillClient.this);
                return;
            }
            try {
                mAutofillPopup =
                        new AutofillPopup(
                                mContext,
                                anchorView,
                                new AutofillDelegate() {
                                    @Override
                                    public void dismissed() {
                                        AndroidAutofillClientJni.get()
                                                .dismissed(
                                                        mNativeAndroidAutofillClient,
                                                        AndroidAutofillClient.this);
                                    }

                                    @Override
                                    public void suggestionSelected(int listIndex) {
                                        AndroidAutofillClientJni.get()
                                                .suggestionSelected(
                                                        mNativeAndroidAutofillClient,
                                                        AndroidAutofillClient.this,
                                                        listIndex);
                                    }

                                    @Override
                                    public void deleteSuggestion(int listIndex) {}

                                    @Override
                                    public void accessibilityFocusCleared() {}
                                },
                                null);
            } catch (RuntimeException e) {
                // Deliberately swallowing exception because bad fraemwork implementation can
                // throw exceptions in ListPopupWindow constructor.
                AndroidAutofillClientJni.get()
                        .dismissed(mNativeAndroidAutofillClient, AndroidAutofillClient.this);
                return;
            }
        }
        mAutofillPopup.filterAndShow(suggestions, isRtl);
    }

    @CalledByNative
    public void hideAutofillPopup() {
        if (mAutofillPopup == null) return;
        mAutofillPopup.dismiss();
        mAutofillPopup = null;
    }

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param name Name of the suggestion.
     * @param label Label of the suggestion.
     * @param uniqueId Unique suggestion id.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(
            AutofillSuggestion[] array,
            int index,
            String name,
            String label,
            @PopupItemId int popupItemId) {
        array[index] =
                new AutofillSuggestion.Builder()
                        .setLabel(name)
                        .setSecondarySubLabel(label)
                        .setItemTag("")
                        .setPopupItemId(popupItemId)
                        .setFeatureForIPH("")
                        .build();
    }

    @NativeMethods
    interface Natives {
        void dismissed(long nativeAndroidAutofillClient, AndroidAutofillClient caller);

        void suggestionSelected(
                long nativeAndroidAutofillClient, AndroidAutofillClient caller, int position);
    }
}
