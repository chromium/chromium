// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.components.autofill_assistant.AssistantEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.components.autofill_assistant.R;

import java.util.List;

/**
 * The shipping address section of the Autofill Assistant payment request.
 */
public class AssistantShippingAddressSection extends AssistantCollectUserDataSection<AddressModel> {
    @Nullable
    private AssistantEditor<AddressModel> mEditor;

    AssistantShippingAddressSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_address_summary,
                R.layout.autofill_assistant_address_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_add_address),
                context.getString(R.string.payments_add_address));
    }

    @Override
    @Nullable
    protected AssistantEditor<AddressModel> getEditor() {
        return mEditor;
    }

    public void setEditor(AssistantEditor<AddressModel> editor) {
        mEditor = editor;
        updateVisibility();
    }

    @Override
    protected void updateFullView(View fullView, @Nullable AddressModel model) {
        if (model == null) {
            return;
        }
        TextView fullNameView = fullView.findViewById(R.id.full_name);
        fullNameView.setText(model.mOption.getFullName());
        hideIfEmpty(fullNameView);

        TextView fullAddressView = fullView.findViewById(R.id.full_address);
        fullAddressView.setText(model.getFullDescription());
        hideIfEmpty(fullAddressView);

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
    protected void updateSummaryView(View summaryView, @Nullable AddressModel model) {
        if (model == null) {
            return;
        }
        TextView fullNameView = summaryView.findViewById(R.id.full_name);
        fullNameView.setText(model.mOption.getFullName());
        hideIfEmpty(fullNameView);

        TextView shortAddressView = summaryView.findViewById(R.id.short_address);
        shortAddressView.setText(model.getSummaryDescription());
        hideIfEmpty(shortAddressView);

        TextView errorView = summaryView.findViewById(R.id.incomplete_error);
        errorView.setVisibility(model.mErrors.isEmpty() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(AddressModel model) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(AddressModel model) {
        return mContext.getString(R.string.payments_edit_address);
    }

    @Override
    protected boolean areEqual(AddressModel modelA, AddressModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        return TextUtils.equals(modelA.mOption.getGUID(), modelB.mOption.getGUID());
    }

    /**
     * The Chrome profiles have changed externally. This will rebuild the UI with the new/changed
     * set of addresses derived from the profiles, while keeping the selected item if possible.
     */
    void onAddressesChanged(List<AddressModel> addresses) {
        if (shouldIgnoreItemChangeNotification()) {
            return;
        }

        int selectedAddressIndex = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < addresses.size(); i++) {
                if (areEqual(addresses.get(i), mSelectedOption)) {
                    selectedAddressIndex = i;
                    break;
                }
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(addresses, selectedAddressIndex);
    }
}
