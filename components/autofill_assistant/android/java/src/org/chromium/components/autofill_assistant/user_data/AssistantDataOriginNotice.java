// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;

/**
 * Shows a link for the user to open an information dialog on the origin of their data.
 */
public class AssistantDataOriginNotice {
    private final View mView;
    private final TextView mLinkToDataOriginDialog;

    AssistantDataOriginNotice(Context context, ViewGroup parent) {
        mView = LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_data_origin_notice, parent, false);
        parent.addView(mView);
        mLinkToDataOriginDialog = mView.findViewById(R.id.link_to_data_origin_dialog);
    }

    void setDataOriginLinkText(String text) {
        if (TextUtils.isEmpty(text)) {
            mView.setVisibility(View.GONE);
        } else {
            mView.setVisibility(View.VISIBLE);
            AssistantTextUtils.applyVisualAppearanceTags(
                    mLinkToDataOriginDialog, text, unused -> {});
        }
    }

    View getView() {
        return mView;
    }
}
