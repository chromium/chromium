// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import static org.chromium.components.autofill_assistant.AssistantAccessibilityUtils.setAccessibility;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.user_data.AssistantCollectUserDataModel.LoginChoiceModel;

import java.util.List;

/**
 * The login details section of the Autofill Assistant payment request.
 */
public class AssistantLoginSection extends AssistantCollectUserDataSection<LoginChoiceModel> {
    AssistantLoginSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_login, R.layout.autofill_assistant_login,
                context.getResources().getDimensionPixelSize(
                        org.chromium.components.autofill_assistant.R.dimen
                                .autofill_assistant_payment_request_title_padding),
                /*titleAddButton=*/null, /*listAddButton=*/null);
    }

    @Override
    protected void createOrEditItem(@Nullable LoginChoiceModel oldItem) {
        assert oldItem != null;
        assert oldItem.mOption.getInfoPopup() != null;

        oldItem.mOption.getInfoPopup().show(mContext);
    }

    @Override
    protected void updateFullView(View fullView, LoginChoiceModel model) {
        updateSummaryView(fullView, model);
    }

    @Override
    protected void updateSummaryView(View summaryView, LoginChoiceModel model) {
        AssistantLoginChoice option = model.mOption;
        TextView labelView = summaryView.findViewById(R.id.label);
        labelView.setText(option.getLabel());
        TextView sublabelView = summaryView.findViewById(R.id.sublabel);
        if (TextUtils.isEmpty(option.getSublabel())) {
            sublabelView.setVisibility(View.GONE);
        } else {
            sublabelView.setText(option.getSublabel());
            setAccessibility(sublabelView, option.getSublabelAccessibilityHint());
        }
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(LoginChoiceModel model) {
        return R.drawable.btn_info;
    }

    @Override
    protected String getEditButtonContentDescription(LoginChoiceModel model) {
        if (model.mOption.getEditButtonContentDescription() != null) {
            return model.mOption.getEditButtonContentDescription();
        } else {
            return mContext.getString(R.string.learn_more);
        }
    }

    @Override
    protected boolean areEqual(
            @Nullable LoginChoiceModel modelA, @Nullable LoginChoiceModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        // Native ensures that each login choice has a unique identifier.
        return TextUtils.equals(modelA.mOption.getIdentifier(), modelB.mOption.getIdentifier());
    }

    /**
     * The login options have changed externally. This will rebuild the UI with the new/changed
     * set of login options, while keeping the selected item if possible.
     */
    void onLoginsChanged(List<LoginChoiceModel> options) {
        int indexToSelect = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < getItems().size(); i++) {
                if (areEqual(mSelectedOption, getItems().get(i))) {
                    indexToSelect = i;
                    break;
                }
            }
        }

        setItems(options, indexToSelect);
    }
}
