// Copyright 2022 The Chromium Authors. All rights reserved.
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
import org.chromium.components.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.components.autofill_assistant.R;

import java.util.List;

/**
 * The phone number section of the Autofill Assistant payment request.
 */
public class AssistantPhoneNumberSection extends AssistantCollectUserDataSection<ContactModel> {
    @Nullable
    private AssistantEditor<ContactModel> mEditor;

    AssistantPhoneNumberSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_contact_summary,
                R.layout.autofill_assistant_contact_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_add_phone_number),
                context.getString(R.string.payments_add_phone_number));
    }

    @Override
    @Nullable
    protected AssistantEditor<ContactModel> getEditor() {
        return mEditor;
    }

    public void setEditor(@Nullable AssistantEditor<ContactModel> editor) {
        mEditor = editor;
        updateVisibility();
    }

    @Override
    protected void updateFullView(View fullView, @Nullable ContactModel model) {
        if (model == null) {
            return;
        }

        TextView fullViewText = fullView.findViewById(R.id.contact_full);
        String description = model.mOption.getPhoneNumber();
        fullViewText.setText(description);
        hideIfEmpty(fullViewText);

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
    protected void updateSummaryView(View summaryView, @Nullable ContactModel model) {
        if (model == null) {
            return;
        }

        TextView contactSummaryView = summaryView.findViewById(R.id.contact_summary);
        String description = model.mOption.getPhoneNumber();
        contactSummaryView.setText(description);
        hideIfEmpty(contactSummaryView);

        summaryView.findViewById(R.id.incomplete_error)
                .setVisibility(model.mErrors.isEmpty() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(ContactModel model) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(ContactModel model) {
        return mContext.getString(R.string.payments_edit_button);
    }

    @Override
    protected boolean areEqual(@Nullable ContactModel modelA, @Nullable ContactModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        return TextUtils.equals(modelA.mOption.getGUID(), modelB.mOption.getGUID());
    }

    /**
     * The Chrome profiles have changed externally. This will rebuild the UI with the new/changed
     * set of phone numbers derived from the profiles, while keeping the selected item if possible.
     */
    void onPhoneNumbersChanged(List<ContactModel> phoneNumbers) {
        if (shouldIgnoreItemChangeNotification()) {
            return;
        }

        int selectedPhoneNumberIndex = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < phoneNumbers.size(); i++) {
                if (areEqual(phoneNumbers.get(i), mSelectedOption)) {
                    selectedPhoneNumberIndex = i;
                    break;
                }
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(phoneNumbers, selectedPhoneNumberIndex);
    }
}
