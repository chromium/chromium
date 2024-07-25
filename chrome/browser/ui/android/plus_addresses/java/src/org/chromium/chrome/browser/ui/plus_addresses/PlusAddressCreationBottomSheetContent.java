// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

/** Implements the content for the plus address creation bottom sheet. */
public class PlusAddressCreationBottomSheetContent implements BottomSheetContent {
    private final ViewGroup mContentView;
    private final LoadingView mLoadingView;
    private final TextView mProposedPlusAddress;
    // The clickable icon used to refresh the suggested plus address.
    private final ImageView mRefreshIcon;
    // The button to confirm the proposed plus address.
    private final Button mPlusAddressConfirmButton;
    // The button to cancel the plus address creation dialog. Only visible on
    // first use, i.e., when there is a notice screen.
    private final Button mPlusAddressCancelButton;
    // Whether we are showing a notice.
    private final boolean mShowingNotice;
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
            @Nullable String plusAddressNotice,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            @Nullable String plusAddressModalCancelText,
            String errorReportInstruction,
            GURL learnMoreUrl,
            GURL errorReportUrl,
            boolean refreshSupported) {
        mShowingNotice = plusAddressNotice != null;
        View layout =
                LayoutInflater.from(activity)
                        .inflate(R.layout.plus_address_creation_prompt, /* root= */ null);
        assert (layout instanceof ViewGroup) : "layout is not a ViewGroup!";
        mContentView = (ViewGroup) layout;

        mLoadingView = mContentView.findViewById(R.id.plus_address_creation_loading_view);
        mProposedPlusAddress = mContentView.findViewById(R.id.proposed_plus_address);
        mRefreshIcon = mContentView.findViewById(R.id.refresh_plus_address_icon);
        mPlusAddressConfirmButton = mContentView.findViewById(R.id.plus_address_confirm_button);
        mPlusAddressCancelButton = mContentView.findViewById(R.id.plus_address_cancel_button);

        // TODO(b/303054310): Once project exigencies allow for it, convert all of
        // these back to the android view XML.
        TextView modalTitleView = mContentView.findViewById(R.id.plus_address_notice_title);
        modalTitleView.setText(modalTitle);

        TextViewWithClickableSpans plusAddressDescriptionView =
                mContentView.findViewById(R.id.plus_address_modal_explanation);
        plusAddressDescriptionView.setText(plusAddressDescription);

        maybeShowFirstTimeUseNotice(activity, plusAddressNotice, learnMoreUrl);

        mProposedPlusAddress.setText(proposedPlusAddressPlaceholder);

        NoUnderlineClickableSpan errorReportLink =
                new NoUnderlineClickableSpan(
                        activity,
                        v -> {
                            mDelegate.openUrl(errorReportUrl);
                        });
        SpannableString errorReportString =
                SpanApplier.applySpans(
                        errorReportInstruction,
                        new SpanApplier.SpanInfo("<link>", "</link>", errorReportLink));
        TextViewWithClickableSpans plusAddressErrorReportView =
                mContentView.findViewById(R.id.plus_address_modal_error_report);
        plusAddressErrorReportView.setText(errorReportString);
        plusAddressErrorReportView.setMovementMethod(LinkMovementMethod.getInstance());
        plusAddressErrorReportView.setVisibility(View.GONE);

        mPlusAddressConfirmButton.setEnabled(false);
        mPlusAddressConfirmButton.setText(plusAddressModalOkText);
        mPlusAddressConfirmButton.setOnClickListener(
                (View _view) -> {
                    showConfirmationLoadingState();
                    mDelegate.onConfirmRequested();
                });

        mProposedPlusAddress.setTypeface(Typeface.MONOSPACE);
        if (refreshSupported) {
            mRefreshIcon.setVisibility(View.VISIBLE);
            mRefreshIcon.setOnClickListener(
                    v -> {
                        if (mPlusAddressConfirmButton.isEnabled()) {
                            mPlusAddressConfirmButton.setEnabled(false);

                            mProposedPlusAddress.setText(
                                    R.string
                                            .plus_address_model_refresh_temporary_label_content_android);
                            mDelegate.onRefreshClicked();
                        }
                    });
        }

        if (mShowingNotice) {
            mPlusAddressCancelButton.setText(plusAddressModalCancelText);
            mPlusAddressCancelButton.setOnClickListener((View _view) -> mDelegate.onCanceled());
        } else {
            mPlusAddressCancelButton.setVisibility(View.GONE);
        }

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);
    }

    public void setProposedPlusAddress(String proposedPlusAddress) {
        mProposedPlusAddress.setText(proposedPlusAddress);
        mPlusAddressConfirmButton.setEnabled(true);
    }

    /** Adjusts the UI to show the loading state for confirming the proposed plus address. */
    public void showConfirmationLoadingState() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PLUS_ADDRESS_LOADING_STATES_ANDROID)) {
            // This also changes the color of the refresh icon to disabled.
            mRefreshIcon.setEnabled(false);

            // Hide the buttons.
            mPlusAddressConfirmButton.setVisibility(View.GONE);
            mPlusAddressCancelButton.setVisibility(View.GONE);
        }
        showLoadingIndicator();
    }

    public void showError() {
        mContentView.findViewById(R.id.proposed_plus_address_container).setVisibility(View.GONE);
        TextViewWithClickableSpans plusAddressErrorReportView =
                mContentView.findViewById(R.id.plus_address_modal_error_report);
        plusAddressErrorReportView.setVisibility(View.VISIBLE);

        hideLoadingIndicator();

        // Disable Confirm button if attempts to Confirm() fail.
        mPlusAddressConfirmButton.setVisibility(View.VISIBLE);
        mPlusAddressConfirmButton.setEnabled(false);
        if (mShowingNotice) {
            mPlusAddressCancelButton.setVisibility(View.VISIBLE);
        }
    }

    public void hideRefreshButton() {
        mRefreshIcon.setVisibility(View.GONE);
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
        // TODO(crbug.com/40276862): Replace with final version.
        return R.string.plus_address_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/40276862): Replace with final version.
        return R.string.plus_address_bottom_sheet_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/40276862): Replace with final version.
        return R.string.plus_address_bottom_sheet_content_description;
    }

    private void maybeShowFirstTimeUseNotice(
            Activity activity, @Nullable String plusAddressNotice, GURL learnMoreUrl) {
        TextView firstTimeNotice =
                mContentView.findViewById(R.id.plus_address_first_time_use_notice);
        if (plusAddressNotice != null) {
            NoUnderlineClickableSpan settingsLink =
                    new NoUnderlineClickableSpan(
                            activity,
                            v -> {
                                mDelegate.openUrl(learnMoreUrl);
                            });
            SpannableString spannableString =
                    SpanApplier.applySpans(
                            plusAddressNotice,
                            new SpanApplier.SpanInfo("<link>", "</link>", settingsLink));
            firstTimeNotice.setText(spannableString);
            firstTimeNotice.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            firstTimeNotice.setVisibility(View.GONE);
        }
    }

    private void showLoadingIndicator() {
        // We skip the delay because otherwise the height of the bottomsheet
        // is adjusted once on hiding the confirm button and then again after
        // the loading view appears.
        mLoadingView.showLoadingUI(/* skipDelay= */ true);
    }

    private void hideLoadingIndicator() {
        mLoadingView.hideLoadingUI();
    }
}
