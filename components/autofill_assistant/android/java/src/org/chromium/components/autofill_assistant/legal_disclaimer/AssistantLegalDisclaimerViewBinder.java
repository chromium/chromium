// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.legal_disclaimer;

import android.view.View;
import android.widget.TextView;

import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Autofill Assistant Legal Disclaimer Message
 * view. These updates are pulled from the {@link AssistantLegalDisclaimerModel} when a notification
 * of an update is received.
 */
class AssistantLegalDisclaimerViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantLegalDisclaimerModel,
                AssistantLegalDisclaimerViewBinder.ViewHolder, PropertyKey> {
    /** A wrapper class that holds the different views of the legal disclaimer. */
    static class ViewHolder {
        final View mRootView;
        final TextView mMessageView;

        public ViewHolder(View rootView) {
            mRootView = rootView;
            mMessageView = mRootView.findViewById(R.id.legal_disclaimer_text);
        }
    }

    @Override
    public void bind(
            AssistantLegalDisclaimerModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantLegalDisclaimerModel.LEGAL_DISCLAIMER == propertyKey) {
            AssistantLegalDisclaimer legalDisclaimer =
                    model.get(AssistantLegalDisclaimerModel.LEGAL_DISCLAIMER);
            if (legalDisclaimer == null) {
                view.mRootView.setVisibility(View.GONE);
                return;
            }
            AssistantLegalDisclaimerDelegate delegate = legalDisclaimer.getDelegate();
            AssistantTextUtils.applyVisualAppearanceTags(
                    view.mMessageView, legalDisclaimer.getMessage(), delegate::onLinkClicked);
            view.mRootView.setVisibility(View.VISIBLE);
        }
    }
}
