// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The modal dialog implementation of the ConsentUI shown to users upon first using Assistant Voice
 * Search.
 */
class AssistantVoiceSearchConsentModal implements AssistantVoiceSearchConsentUi {
    private View mContentView;
    private AssistantVoiceSearchConsentUi.Observer mObserver;

    private ModalDialogManager mModalDialogManager;
    private PropertyModel mConsentModal;

    public AssistantVoiceSearchConsentModal(
            @NonNull Context context, @NonNull ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;

        mContentView = LayoutInflater.from(context).inflate(
                R.layout.assistant_voice_search_modal_consent_ui, /* root= */ null);

        View learnMore = mContentView.findViewById(R.id.avs_consent_ui_learn_more);
        learnMore.setOnClickListener((v) -> mObserver.onLearnMoreClicked());

        Resources resources = context.getResources();
        mConsentModal =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        mModalDialogManager, this::onDismissConsentModal))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.avs_consent_ui_simplified_accept)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.avs_consent_ui_simplified_deny)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mContentView)
                        .build();
    }

    // AssistantVoiceSearchConsentUi implementation.

    @Override
    public void show(AssistantVoiceSearchConsentUi.Observer observer) {
        assert mObserver == null;

        mObserver = observer;
        mModalDialogManager.showDialog(mConsentModal, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void dismiss() {
        mObserver = null;
        mModalDialogManager.dismissDialog(mConsentModal, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    private void onDismissConsentModal(@DialogDismissalCause int dismissalCause) {
        if (mObserver == null) {
            // The observer was already notified of a result, so the UI was dismissed by the
            // controller.
            return;
        }

        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            mObserver.onConsentAccepted();
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            mObserver.onConsentRejected();
        } else if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            mObserver.onConsentCanceled();
        } else {
            mObserver.onNonUserCancel();
        }
        mObserver = null;
    }
}
