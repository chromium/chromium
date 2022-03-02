// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill_assistant.AssistantAutofillCreditCard;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;
import org.chromium.components.autofill_assistant.AssistantOptionModel;
import org.chromium.components.autofill_assistant.generic_ui.AssistantValue;

/** Delegate for the Collect user data UI which forwards events to a native counterpart. */
@JNINamespace("autofill_assistant")
public class AssistantCollectUserDataNativeDelegate implements AssistantCollectUserDataDelegate {
    private long mNativeAssistantCollectUserDataDelegate;

    @CalledByNative
    private static AssistantCollectUserDataNativeDelegate create(
            long nativeAssistantCollectUserDataDelegate) {
        return new AssistantCollectUserDataNativeDelegate(nativeAssistantCollectUserDataDelegate);
    }

    private AssistantCollectUserDataNativeDelegate(long nativeAssistantCollectUserDataDelegate) {
        mNativeAssistantCollectUserDataDelegate = nativeAssistantCollectUserDataDelegate;
    }

    @Override
    public void onContactInfoChanged(@Nullable AssistantOptionModel.ContactModel contactModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onContactInfoChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    contactModel == null ? null : contactModel.mOption, eventType);
        }
    }

    @Override
    public void onPhoneNumberChanged(@Nullable AssistantOptionModel.ContactModel contactModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onPhoneNumberChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    contactModel == null ? null : contactModel.mOption, eventType);
        }
    }

    @Override
    public void onShippingAddressChanged(@Nullable AssistantOptionModel.AddressModel addressModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onShippingAddressChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    addressModel == null ? null : addressModel.mOption, eventType);
        }
    }

    @Override
    public void onPaymentMethodChanged(
            @Nullable AssistantOptionModel.PaymentInstrumentModel paymentInstrumentModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onCreditCardChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    paymentInstrumentModel == null ? null
                                                   : paymentInstrumentModel.mOption.getCreditCard(),
                    paymentInstrumentModel == null
                            ? null
                            : paymentInstrumentModel.mOption.getBillingAddress(),
                    eventType);
        }
    }

    @Override
    public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onTermsAndConditionsChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, state);
        }
    }

    @Override
    public void onTextLinkClicked(int link) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onTextLinkClicked(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, link);
        }
    }

    @Override
    public void onLoginChoiceChanged(
            @Nullable AssistantCollectUserDataModel.LoginChoiceModel loginChoiceModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onLoginChoiceChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    loginChoiceModel == null ? null : loginChoiceModel.mOption.getIdentifier(),
                    eventType);
        }
    }

    @Override
    public void onKeyValueChanged(String key, AssistantValue value) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onKeyValueChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, key, value);
        }
    }

    @Override
    public void onInputTextFocusChanged(boolean isFocused) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onInputTextFocusChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, isFocused);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantCollectUserDataDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onContactInfoChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable AssistantAutofillProfile contactProfile, int eventType);
        void onPhoneNumberChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable AssistantAutofillProfile phoneNumber, int eventType);
        void onShippingAddressChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable AssistantAutofillProfile address, int eventType);
        void onCreditCardChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable AssistantAutofillCreditCard card,
                @Nullable AssistantAutofillProfile billingProfile, int eventType);
        void onTermsAndConditionsChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int state);
        void onTextLinkClicked(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int link);
        void onLoginChoiceChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, String choice, int eventType);
        void onKeyValueChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, String key, AssistantValue value);
        void onInputTextFocusChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, boolean isFocused);
    }
}
