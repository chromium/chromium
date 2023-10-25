// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;
import android.text.SpannableString;
import android.text.style.TextAppearanceSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Implements the content for the plus address creation bottom sheet. */
public class PlusAddressCreationBottomSheetContent implements BottomSheetContent {
    private final View mView;
    private final PlusAddressCreationDelegate mDelegate;

    /**
     * Creates the BottomSheetContent and inflates the view given a delegate responding to actions.
     */
    public PlusAddressCreationBottomSheetContent(
            PlusAddressCreationDelegate delegate,
            Activity activity,
            String modalTitle,
            String plusAddressDescription,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            String plusAddressModalCancelText) {
        mView =
                LayoutInflater.from(activity)
                        .inflate(R.layout.plus_address_creation_prompt, /* root= */ null);
        mDelegate = delegate;

        // TODO(b/303054310): Once project exigencies allow for it, convert all of
        // these back to the android view XML.
        TextView modalTitleView = mView.findViewById(R.id.plus_address_notice_title);
        modalTitleView.setText(modalTitle);

        // TODO(crbug.com/1467623): Link to Plus Address account settings
        NoUnderlineClickableSpan settingsLink = new NoUnderlineClickableSpan(activity, v -> {});
        TextAppearanceSpan boldText =
                new TextAppearanceSpan(activity, R.style.TextAppearance_TextMediumThick_Secondary);
        SpannableString spannableString =
                SpanApplier.applySpans(
                        plusAddressDescription,
                        new SpanApplier.SpanInfo("<link>", "</link>", settingsLink),
                        new SpanApplier.SpanInfo("<b>", "</b>", boldText));
        TextView plusAddressDescriptionView =
                mView.findViewById(R.id.plus_address_modal_explanation);
        plusAddressDescriptionView.setText(spannableString);

        TextView proposedPlusAddressView = mView.findViewById(R.id.proposed_plus_address);
        proposedPlusAddressView.setText(proposedPlusAddressPlaceholder);

        Button plusAddressConfirmButton = mView.findViewById(R.id.plus_address_confirm_button);
        plusAddressConfirmButton.setText(plusAddressModalOkText);
        plusAddressConfirmButton.setOnClickListener((View _view) -> mDelegate.onConfirmed());

        Button plusAddressCancelButton = mView.findViewById(R.id.plus_address_cancel_button);
        plusAddressCancelButton.setText(plusAddressModalCancelText);
        plusAddressCancelButton.setOnClickListener((View _view) -> mDelegate.onCanceled());
    }

    // BottomSheetContent implementation follows:
    @Override
    public View getContentView() {
        return mView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mView.getScrollY();
    }

    @Override
    public void destroy() {
        // Cleanup is handled by PlusAddressCreationViewBridge.onSheetClosed
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(crbug.com/1467623): Replace with final version.
        return R.string.plus_address_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/1467623): Replace with final version.
        return R.string.plus_address_bottom_sheet_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/1467623): Replace with final version.
        return R.string.plus_address_bottom_sheet_content_description;
    }
}
