// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.TextAppearanceSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

/** Implements the content for the plus address creation bottom sheet. */
public class PlusAddressCreationBottomSheetContent implements BottomSheetContent {
    private final ViewGroup mContentView;
    private final LoadingView mLoadingView;
    private boolean mShowingLoadingView;
    private PlusAddressCreationDelegate mDelegate;

    /**
     * Creates the BottomSheetContent and inflates the view given a delegate responding to actions.
     *
     * <p>The confirm and cancel button on-click listeners rely on the existence of the delegate, so
     * setDelegate must be called before handling those click events.
     */
    public PlusAddressCreationBottomSheetContent(
            Activity activity,
            String modalTitle,
            String plusAddressDescription,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            String plusAddressModalCancelText,
            GURL manageUrl) {
        View layout =
                LayoutInflater.from(activity)
                        .inflate(R.layout.plus_address_creation_prompt, /* root= */ null);
        assert (layout instanceof ViewGroup) : "layout is not a ViewGroup!";
        mContentView = (ViewGroup) layout;
        mLoadingView = new LoadingView(activity);
        mLoadingView.setVisibility(View.GONE);
        mContentView.addView(mLoadingView);

        // TODO(b/303054310): Once project exigencies allow for it, convert all of
        // these back to the android view XML.
        TextView modalTitleView = mContentView.findViewById(R.id.plus_address_notice_title);
        modalTitleView.setText(modalTitle);

        ImageView logoView = (ImageView) mContentView.findViewById(R.id.plus_address_logo);
        logoView.setImageResource(R.drawable.plus_addresses_logo);

        NoUnderlineClickableSpan settingsLink =
                new NoUnderlineClickableSpan(
                        activity,
                        v -> {
                            mDelegate.openManagementPage(manageUrl);
                        });
        TextAppearanceSpan boldText =
                new TextAppearanceSpan(activity, R.style.TextAppearance_TextMediumThick_Secondary);

        SpannableString spannableString =
                SpanApplier.applySpans(
                        plusAddressDescription,
                        new SpanApplier.SpanInfo("<link>", "</link>", settingsLink),
                        new SpanApplier.SpanInfo("<b>", "</b>", boldText));

        TextViewWithClickableSpans plusAddressDescriptionView =
                mContentView.findViewById(R.id.plus_address_modal_explanation);
        plusAddressDescriptionView.setText(spannableString);
        plusAddressDescriptionView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView proposedPlusAddressView = mContentView.findViewById(R.id.proposed_plus_address);
        proposedPlusAddressView.setText(proposedPlusAddressPlaceholder);

        Button plusAddressConfirmButton =
                mContentView.findViewById(R.id.plus_address_confirm_button);
        plusAddressConfirmButton.setText(plusAddressModalOkText);
        plusAddressConfirmButton.setOnClickListener(
                (View _view) -> {
                    showLoadingIndicator();
                    mDelegate.onConfirmRequested();
                });

        Button plusAddressCancelButton = mContentView.findViewById(R.id.plus_address_cancel_button);
        plusAddressCancelButton.setText(plusAddressModalCancelText);
        plusAddressCancelButton.setOnClickListener((View _view) -> mDelegate.onCanceled());
    }

    public void setProposedPlusAddress(String proposedPlusAddress) {
        TextView proposedPlusAddressView = mContentView.findViewById(R.id.proposed_plus_address);
        proposedPlusAddressView.setText(proposedPlusAddress);
        // Enable Confirm button if modal use was blocked up until now.
        Button plusAddressConfirmButton =
                mContentView.findViewById(R.id.plus_address_confirm_button);
        if (!plusAddressConfirmButton.isEnabled()) {
            plusAddressConfirmButton.setEnabled(true);
        }
    }

    public void showError(String errorMessage) {
        TextView proposedPlusAddressView = mContentView.findViewById(R.id.proposed_plus_address);
        proposedPlusAddressView.setText(errorMessage);
        // Disable Confirm button if attempts to Confirm() fail.
        Button plusAddressConfirmButton =
                mContentView.findViewById(R.id.plus_address_confirm_button);
        if (plusAddressConfirmButton.isEnabled()) {
            plusAddressConfirmButton.setEnabled(false);
        }
        hideLoadingIndicator();
    }

    /** Sets the delegate listening for actions the user performs on this bottom sheet. */
    public void setDelegate(PlusAddressCreationDelegate delegate) {
        mDelegate = delegate;
    }

    // BottomSheetContent implementation follows:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mContentView.getScrollY();
    }

    @Override
    public void destroy() {
        // Some cleanup is handled by PlusAddressCreationMediator.onSheetClosed
        // TODO: crbug.com/1467623 - Consolidate cleanup behavior.
        mLoadingView.destroy();
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

    public boolean showsLoadingIndicatorForTesting() {
        return mShowingLoadingView;
    }

    private void showLoadingIndicator() {
        mLoadingView.showLoadingUI();
        mShowingLoadingView = true;
    }

    private void hideLoadingIndicator() {
        mLoadingView.hideLoadingUI();
        mShowingLoadingView = false;
    }
}
