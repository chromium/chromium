// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.autofill_assistant.AssistantAutofillCreditCard;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;
import org.chromium.components.autofill_assistant.AssistantEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.PaymentInstrumentModel;
import org.chromium.components.autofill_assistant.AssistantPaymentInstrument;
import org.chromium.components.autofill_assistant.R;

import java.util.List;

/**
 * The payment method section of the Autofill Assistant payment request.
 */
public class AssistantPaymentMethodSection
        extends AssistantCollectUserDataSection<PaymentInstrumentModel> {
    @Nullable
    private AssistantPaymentInstrumentEditor mEditor;

    AssistantPaymentMethodSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_payment_method_summary,
                R.layout.autofill_assistant_payment_method_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_payment_method_title_padding),
                context.getString(R.string.payments_add_card),
                context.getString(R.string.payments_add_card));
        setTitle(context.getString(R.string.payments_method_of_payment_label));
    }

    @Override
    @Nullable
    public AssistantEditor<PaymentInstrumentModel> getEditor() {
        return mEditor;
    }

    public void setEditor(@Nullable AssistantPaymentInstrumentEditor editor) {
        mEditor = editor;
        updateUi();
        if (mEditor == null) {
            return;
        }

        for (PaymentInstrumentModel item : getItems()) {
            AssistantAutofillProfile profile = item.mOption.getBillingAddress();
            if (profile != null) {
                addAutocompleteInformationToEditor(profile);
            }
        }
    }

    @Override
    protected void updateFullView(View fullView, PaymentInstrumentModel model) {
        if (model == null) {
            return;
        }

        updateView(fullView, model);

        TextView cardNameView = fullView.findViewById(R.id.credit_card_name);
        cardNameView.setText(model.mOption.getCreditCard().getName());
        hideIfEmpty(cardNameView);

        TextView errorView = fullView.findViewById(R.id.incomplete_error);
        if (model.mErrors.isEmpty()) {
            errorView.setText("");
            errorView.setVisibility(View.GONE);
        } else {
            errorView.setText(TextUtils.join("\n", model.mErrors));
            errorView.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected void updateSummaryView(View summaryView, PaymentInstrumentModel model) {
        if (model == null) {
            return;
        }

        updateView(summaryView, model);

        TextView errorView = summaryView.findViewById(R.id.incomplete_error);
        errorView.setVisibility(model.mErrors.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void updateView(View view, PaymentInstrumentModel model) {
        AssistantPaymentInstrument method = model.mOption;
        ImageView cardIssuerImageView = view.findViewById(R.id.credit_card_issuer_icon);
        try {
            cardIssuerImageView.setImageDrawable(AppCompatResources.getDrawable(
                    view.getContext(), method.getCreditCard().getIssuerIconDrawableId()));
        } catch (Resources.NotFoundException e) {
            cardIssuerImageView.setImageDrawable(null);
        }

        // By default, the obfuscated number contains the issuer (e.g., 'Visa'). This is needlessly
        // verbose, so we strip it away. See |PersonalDataManagerTest::testAddAndEditCreditCards|
        // for explanation of "\u0020...\u2060".
        String obfuscatedNumber = method.getCreditCard().getObfuscatedNumber();
        int beginningOfObfuscatedNumber =
                Math.max(obfuscatedNumber.indexOf("\u0020\u202A\u2022\u2060"), 0);
        obfuscatedNumber = obfuscatedNumber.substring(beginningOfObfuscatedNumber);
        TextView cardNumberView = view.findViewById(R.id.credit_card_number);
        cardNumberView.setText(obfuscatedNumber);
        hideIfEmpty(cardNumberView);

        TextView cardExpirationView = view.findViewById(R.id.credit_card_expiration);
        cardExpirationView.setText(method.getCreditCard().getFormattedExpirationDate(mContext));
        hideIfEmpty(cardExpirationView);
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(PaymentInstrumentModel model) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(PaymentInstrumentModel model) {
        return mContext.getString(R.string.autofill_edit_credit_card);
    }

    @Override
    protected boolean areEqual(
            @Nullable PaymentInstrumentModel modelA, @Nullable PaymentInstrumentModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        return areEqualCards(modelA.mOption.getCreditCard(), modelB.mOption.getCreditCard())
                && areEqualBillingProfiles(
                        modelA.mOption.getBillingAddress(), modelB.mOption.getBillingAddress());
    }
    private boolean areEqualCards(
            AssistantAutofillCreditCard cardA, AssistantAutofillCreditCard cardB) {
        return TextUtils.equals(cardA.getGUID(), cardB.getGUID());
    }
    private boolean areEqualBillingProfiles(@Nullable AssistantAutofillProfile profileA,
            @Nullable AssistantAutofillProfile profileB) {
        if (profileA == null || profileB == null) {
            return profileA == profileB;
        }
        return TextUtils.equals(profileA.getGUID(), profileB.getGUID());
    }

    void onAddressesChanged(List<AssistantAutofillProfile> addresses) {
        // TODO(crbug.com/806868): replace suggested billing addresses (remove if necessary).
        for (AssistantAutofillProfile address : addresses) {
            addAutocompleteInformationToEditor(address);
        }
    }

    /**
     * The set of available payment methods has changed externally. This will rebuild the UI with
     * the new/changed set of payment methods, while keeping the selected item if possible.
     */
    void onAvailablePaymentMethodsChanged(List<PaymentInstrumentModel> paymentMethods) {
        if (shouldIgnoreItemChangeNotification()) {
            return;
        }

        int selectedMethodIndex = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < paymentMethods.size(); ++i) {
                if (areEqual(paymentMethods.get(i), mSelectedOption)) {
                    selectedMethodIndex = i;
                    break;
                }
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(paymentMethods, selectedMethodIndex);
    }

    private void addAutocompleteInformationToEditor(AssistantAutofillProfile profile) {
        if (mEditor != null) {
            mEditor.addAddressInformationForAutocomplete(profile);
        }
    }
}
