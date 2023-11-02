// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.legal_disclaimer;

import android.content.Context;
import android.view.View;

import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.legal_disclaimer.AssistantLegalDisclaimerViewBinder.ViewHolder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator responsible for showing the LegalDisclaimer. */
public class AssistantLegalDisclaimerCoordinator {
    private final View mRootView;
    private AssistantLegalDisclaimerViewBinder mViewBinder;

    public AssistantLegalDisclaimerCoordinator(
            Context context, AssistantLegalDisclaimerModel model) {
        mRootView = LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_legal_disclaimer, /* root= */ null);
        ViewHolder viewHolder = new ViewHolder(mRootView);
        mViewBinder = new AssistantLegalDisclaimerViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, mViewBinder);
    }

    /** Return the view containing the legal disclaimer message. */
    public View getView() {
        return mRootView;
    }
}
