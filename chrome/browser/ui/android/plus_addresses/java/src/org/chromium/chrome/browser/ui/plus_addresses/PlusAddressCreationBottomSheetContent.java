// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.content.Context;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;

/** Implements the content for the plus address creation bottom sheet. */
public class PlusAddressCreationBottomSheetContent implements BottomSheetContent {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;

    final ViewGroup mContentView;

    // The content of the bottom sheet shown in the normal state, i.e. when no error occurs.
    final ViewGroup mPlusAddressContent;
    final TextView mTitleView;
    final TextView mDescriptionView;
    // The user onboarding message with a clickable link that navigates the user to the feature
    // description page. This is not visible if the user has accepted the onboarding notice on any
    // device for the current account.
    final TextViewWithClickableSpans mFirstTimeNotice;
    // Contains the plus address icon, the proposed plus address text view and the refresh icon
    // if plus address refresh is supported.
    final ViewGroup mProposedPlusAddressContainer;
    // Shows plus address logo before the plus address text view to the user.
    final ImageView mProposedPlusAddressIcon;
    // Shows a loading view, which is visible only when the plus address is being reserved or
    // refreshed.
    final LoadingView mProposedPlusAddressLoadingView;
    // Displays the proposed plus address to the user. This UI string can be updated if plus address
    // refresh is supported and user didn't reach the allocation limit.
    final TextView mProposedPlusAddress;
    // The clickable icon used to refresh the suggested plus address.
    final ImageView mRefreshIcon;
    // Legacy error reporting instruction.
    final TextViewWithClickableSpans mPlusAddressErrorReportView;
    // The button to confirm the proposed plus address.
    final Button mPlusAddressConfirmButton;
    // The button to cancel the plus address creation dialog. Only visible on
    // first use, i.e., when there is a notice screen.
    final Button mPlusAddressCancelButton;
    final LoadingView mLoadingView;

    // The content of the error message screen. This view is shown when the normal state content is
    // hidden.
    final ViewStub mErrorContentStub;

    private PlusAddressCreationDelegate mDelegate;

    /**
     * Creates the BottomSheetContent and inflates the view given a delegate responding to actions.
     *
     * <p>The confirm and cancel button on-click listeners rely on the existence of the delegate, so
     * setDelegate must be called before handling those click events.
     */
    public PlusAddressCreationBottomSheetContent(
            Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;

        View layout =
                LayoutInflater.from(context)
                        .inflate(R.layout.plus_address_creation_prompt, /* root= */ null);
        assert (layout instanceof ViewGroup) : "layout is not a ViewGroup!";
        mContentView = (ViewGroup) layout;

        mPlusAddressContent = mContentView.findViewById(R.id.plus_address_content);

        mTitleView = mContentView.findViewById(R.id.plus_address_notice_title);
        mDescriptionView = mContentView.findViewById(R.id.plus_address_modal_explanation);
        mFirstTimeNotice = mContentView.findViewById(R.id.plus_address_first_time_use_notice);
        mFirstTimeNotice.setMovementMethod(LinkMovementMethod.getInstance());

        mProposedPlusAddressContainer =
                mContentView.findViewById(R.id.proposed_plus_address_container);
        mProposedPlusAddressIcon = mContentView.findViewById(R.id.proposed_plus_address_logo);
        mProposedPlusAddressLoadingView =
                mContentView.findViewById(R.id.proposed_plus_address_loading_view);
        mProposedPlusAddressLoadingView.addObserver(
                new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {}

                    @Override
                    public void onHideLoadingUIComplete() {
                        if (mDelegate != null) {
                            mContentView.post(mDelegate::onPlusAddressLoadingViewHidden);
                        }
                    }
                });
        mProposedPlusAddress = mContentView.findViewById(R.id.proposed_plus_address);
        mProposedPlusAddress.setTypeface(Typeface.MONOSPACE);

        mPlusAddressErrorReportView =
                mContentView.findViewById(R.id.plus_address_modal_error_report);
        mPlusAddressErrorReportView.setMovementMethod(LinkMovementMethod.getInstance());

        mRefreshIcon = mContentView.findViewById(R.id.refresh_plus_address_icon);

        mPlusAddressConfirmButton = mContentView.findViewById(R.id.plus_address_confirm_button);
        mPlusAddressConfirmButton.setOnClickListener(unused -> mDelegate.onConfirmRequested());

        mPlusAddressCancelButton = mContentView.findViewById(R.id.plus_address_cancel_button);
        mPlusAddressCancelButton.setOnClickListener(unused -> mDelegate.onCanceled());

        mLoadingView = mContentView.findViewById(R.id.plus_address_creation_loading_view);
        // {@link LoadingView} is shown and hidden with a delay. This prevents the bottom sheet to
        // adjust its height automatically. This observer ensures that the bottom sheet height is
        // adjusted after every loading view state change event.
        mLoadingView.addObserver(
                new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {}

                    @Override
                    public void onHideLoadingUIComplete() {
                        if (mDelegate != null) {
                            mDelegate.onConfirmationLoadingViewHidden();
                        }
                    }
                });

        mErrorContentStub = mContentView.findViewById(R.id.plus_address_error_container_stub);

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);
    }

    void setOnboardingNotice(String notice, GURL learnMoreUrl) {
        NoUnderlineClickableSpan settingsLink =
                new NoUnderlineClickableSpan(
                        mContext,
                        v -> {
                            mDelegate.openUrl(learnMoreUrl);
                        });
        SpannableString spannableString =
                SpanApplier.applySpans(
                        notice, new SpanApplier.SpanInfo("<link>", "</link>", settingsLink));
        mFirstTimeNotice.setText(spannableString);
        mFirstTimeNotice.setVisibility(View.VISIBLE);
    }

    void setVisible(boolean visible) {
        if (visible) {
            mBottomSheetController.requestShowContent(this, /* animate= */ true);
        } else {
            mBottomSheetController.hideContent(this, /* animate= */ true);
        }
    }

    void setPlusAddressIconVisible(boolean visible) {
        mProposedPlusAddressIcon.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setPlusAddressLoadingViewVisible(boolean visible) {
        if (visible) {
            mProposedPlusAddressLoadingView.showLoadingUI(/* skipDelay= */ true);
        } else {
            mProposedPlusAddressLoadingView.hideLoadingUI();
        }
    }

    void setProposedPlusAddress(String proposedPlusAddress) {
        mProposedPlusAddress.setText(proposedPlusAddress);
    }

    void setRefreshIconEnabled(boolean enabled) {
        mRefreshIcon.setEnabled(enabled);
        if (enabled) {
            mRefreshIcon.setOnClickListener(unused -> mDelegate.onRefreshClicked());
        } else {
            mRefreshIcon.setOnClickListener(null);
        }
    }

    void setRefreshIconVisible(boolean visible) {
        mRefreshIcon.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setConfirmButtonEnabled(boolean enabled) {
        mPlusAddressConfirmButton.setEnabled(enabled);
    }

    void setConfirmButtonVisible(boolean visible) {
        mPlusAddressConfirmButton.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setCancelButtonVisible(boolean visible) {
        mPlusAddressCancelButton.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets the delegate listening for actions the user performs on this bottom sheet. */
    void setDelegate(PlusAddressCreationDelegate delegate) {
        mDelegate = delegate;
    }

    void setLegacyErrorReportingInstructionVisible(boolean visible) {
        mProposedPlusAddressContainer.setVisibility(visible ? View.GONE : View.VISIBLE);
        mPlusAddressErrorReportView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setLegacyErrorReportingInstruction(String intruction, GURL errorReportUrl) {
        NoUnderlineClickableSpan errorReportLink =
                new NoUnderlineClickableSpan(
                        mContext,
                        v -> {
                            mDelegate.openUrl(errorReportUrl);
                        });
        SpannableString errorReportString =
                SpanApplier.applySpans(
                        intruction, new SpanApplier.SpanInfo("<link>", "</link>", errorReportLink));
        mPlusAddressErrorReportView.setText(errorReportString);
    }

    void setLoadingIndicatorVisible(boolean visible) {
        if (visible) {
            // We skip the delay because otherwise the height of the bottomsheet
            // is adjusted once on hiding the confirm button and then again after
            // the loading view appears.
            mLoadingView.showLoadingUI(/* skipDelay= */ true);
        } else {
            mLoadingView.hideLoadingUI();
        }
    }

    void expandSheet() {
        mBottomSheetController.expandSheet();
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
}
