// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * The bottom sheet implementation of the ConsentUI shown to users upon first using Assistant Voice
 * Search.
 */
class AssistantVoiceSearchConsentBottomSheet
        implements BottomSheetContent, AssistantVoiceSearchConsentUi {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    private AssistantVoiceSearchConsentUi.Observer mObserver;
    private View mContentView;

    public AssistantVoiceSearchConsentBottomSheet(
            @NonNull Context context, @Nullable BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;

        if (FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ASSISTANT_CONSENT_SIMPLIFIED_TEXT)) {
            mContentView = LayoutInflater.from(context).inflate(
                    R.layout.assistant_voice_search_simplified_consent_ui, /* root= */ null);
        } else {
            mContentView = LayoutInflater.from(context).inflate(
                    R.layout.assistant_voice_search_consent_ui, /* root= */ null);
        }

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                if (mObserver == null) {
                    // The observer was already notified of a result.
                    return;
                }

                if (reason == BottomSheetController.StateChangeReason.TAP_SCRIM
                        || reason == BottomSheetController.StateChangeReason.BACK_PRESS) {
                    mObserver.onConsentCanceled();
                } else {
                    // The sheet was closed by a non-user action.
                    mObserver.onNonUserCancel();
                }
                mObserver = null;
            }
        };

        View acceptButton = mContentView.findViewById(R.id.button_primary);
        acceptButton.setOnClickListener((v) -> {
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
            mObserver.onConsentAccepted();
            mObserver = null;
        });

        View cancelButton = mContentView.findViewById(R.id.button_secondary);
        cancelButton.setOnClickListener((v) -> {
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
            mObserver.onConsentRejected();
            mObserver = null;
        });

        View learnMore = mContentView.findViewById(R.id.avs_consent_ui_learn_more);
        learnMore.setOnClickListener((v) -> mObserver.onLearnMoreClicked());
    }

    // AssistantVoiceSearchConsentUi implementation.

    @Override
    public void show(AssistantVoiceSearchConsentUi.Observer observer) {
        assert mObserver == null;
        assert !mBottomSheetController.isSheetOpen();

        mObserver = observer;
        if (!mBottomSheetController.requestShowContent(this, /* animate= */ true)) {
            mBottomSheetController.hideContent(
                    this, /* animate= */ false, BottomSheetController.StateChangeReason.NONE);
            destroy();
            return;
        }

        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    @Override
    public void dismiss() {
        mObserver = null;
        mBottomSheetController.hideContent(this, /* animate= */ true,
                BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }

    // BottomSheetContent implementation.

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        if (mObserver != null) {
            // The sheet was destroyed before the observer was notified. Count as a non-user cancel.
            mObserver.onNonUserCancel();
            mObserver = null;
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    public @ContentPriority int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        // Disable the peeking behavior for this dialog.
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.avs_consent_ui_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.avs_consent_ui_half_height_description;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.avs_consent_ui_full_height_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.avs_consent_ui_closed_description;
    }
}
