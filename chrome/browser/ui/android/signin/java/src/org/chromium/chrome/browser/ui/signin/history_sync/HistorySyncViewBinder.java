// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.signin.ConsentTextTracker;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

class HistorySyncViewBinder {
    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    public static void bind(PropertyModel model, HistorySyncView view, PropertyKey key) {
        if (key == HistorySyncProperties.PROFILE_DATA) {
            view.getAccountImageView()
                    .setImageDrawable(model.get(HistorySyncProperties.PROFILE_DATA).getImage());
        } else if (key == HistorySyncProperties.ON_ACCEPT_CLICKED) {
            view.getAcceptButton()
                    .setOnClickListener(model.get(HistorySyncProperties.ON_ACCEPT_CLICKED));
        } else if (key == HistorySyncProperties.ON_DECLINE_CLICKED) {
            view.getDeclineButton()
                    .setOnClickListener(model.get(HistorySyncProperties.ON_DECLINE_CLICKED));
        } else if (key == HistorySyncProperties.ON_MORE_CLICKED) {
            view.getMoreButton()
                    .setOnClickListener(model.get(HistorySyncProperties.ON_MORE_CLICKED));
        } else if (key == HistorySyncProperties.ON_SETTINGS_CLICKED) {
            updateSigninDetailsDescription(
                    view.getDetailsDescriptionView(),
                    model.get(HistorySyncProperties.ON_SETTINGS_CLICKED));
        } else {
            throw new IllegalArgumentException("Unknown property key: " + key);
        }
    }

    private static void updateSigninDetailsDescription(
            TextView textView, View.OnClickListener onSettingsLinkClicked) {
        ConsentTextTracker consentTextTracker = new ConsentTextTracker(textView.getResources());
        final @Nullable Object settingsLinkSpan =
                new NoUnderlineClickableSpan(textView.getContext(), onSettingsLinkClicked::onClick);
        final SpanApplier.SpanInfo spanInfo =
                new SpanApplier.SpanInfo(SETTINGS_LINK_OPEN, SETTINGS_LINK_CLOSE, settingsLinkSpan);
        consentTextTracker.setText(
                textView,
                R.string.sync_consent_details_description,
                input -> SpanApplier.applySpans(input.toString(), spanInfo));
    }
}
