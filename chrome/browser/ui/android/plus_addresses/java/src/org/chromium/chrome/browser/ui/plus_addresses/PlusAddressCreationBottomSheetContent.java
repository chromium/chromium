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
    private final ViewGroup mContentView;
    private final LoadingView mLoadingView;
    private final TextView mProposedPlusAddress;
    // The clickable icon used to refresh the suggested plus address.
    private final ImageView mRefreshIcon;
    // Legacy error reporting instruction.
    private final TextViewWithClickableSpans mPlusAddressErrorReportView;
    // The button to confirm the proposed plus address.
    private final Button mPlusAddressConfirmButton;
    // The button to cancel the plus address creation dialog. Only visible on
    // first use, i.e., when there is a notice screen.
    private final Button mPlusAddressCancelButton;
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

        mLoadingView = mContentView.findViewById(R.id.plus_address_creation_loading_view);
        mProposedPlusAddress = mContentView.findViewById(R.id.proposed_plus_address);
        mRefreshIcon = mContentView.findViewById(R.id.refresh_plus_address_icon);
        mPlusAddressConfirmButton = mContentView.findViewById(R.id.plus_address_confirm_button);
        mPlusAddressCancelButton = mContentView.findViewById(R.id.plus_address_cancel_button);

        mPlusAddressErrorReportView =
                mContentView.findViewById(R.id.plus_address_modal_error_report);
        mPlusAddressErrorReportView.setMovementMethod(LinkMovementMethod.getInstance());
        mPlusAddressErrorReportView.setVisibility(View.GONE);

        mProposedPlusAddress.setTypeface(Typeface.MONOSPACE);

        mPlusAddressConfirmButton.setOnClickListener(unused -> mDelegate.onConfirmRequested());
        mPlusAddressCancelButton.setOnClickListener(unused -> mDelegate.onCanceled());

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);
    }

    void setNormalStateInfo(PlusAddressCreationNormalStateInfo info) {
        // TODO(b/303054310): Once project exigencies allow for it, convert all of
        // these back to the android view XML.
        TextView modalTitleView = mContentView.findViewById(R.id.plus_address_notice_title);
        modalTitleView.setText(info.getTitle());

        TextViewWithClickableSpans plusAddressDescriptionView =
                mContentView.findViewById(R.id.plus_address_modal_explanation);
        plusAddressDescriptionView.setText(info.getDescription());

        maybeShowFirstTimeUseNotice(info.getNotice(), info.getLearnMoreUrl());

        NoUnderlineClickableSpan errorReportLink =
                new NoUnderlineClickableSpan(
                        mContext,
                        v -> {
                            mDelegate.openUrl(info.getErrorReportUrl());
                        });
        SpannableString errorReportString =
                SpanApplier.applySpans(
                        info.getErrorReportInstruction(),
                        new SpanApplier.SpanInfo("<link>", "</link>", errorReportLink));
        mPlusAddressErrorReportView.setText(errorReportString);

        mPlusAddressConfirmButton.setText(info.getConfirmText());
        mPlusAddressCancelButton.setText(info.getCancelText());
    }

    void setVisible(boolean visible) {
        if (visible) {
            mBottomSheetController.requestShowContent(this, /* animate= */ true);
        } else {
            mBottomSheetController.hideContent(this, /* animate= */ true);
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
        mContentView
                .findViewById(R.id.proposed_plus_address_container)
                .setVisibility(visible ? View.GONE : View.VISIBLE);
        mPlusAddressErrorReportView.setVisibility(visible ? View.VISIBLE : View.GONE);
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

    void setErrorStateInfo(PlusAddressCreationErrorStateInfo errorStateInfo) {
        View plusAddressContent = mContentView.findViewById(R.id.plus_address_content);
        plusAddressContent.setVisibility(View.GONE);

        ViewStub errorContentStub =
                mContentView.findViewById(R.id.plus_address_error_container_stub);
        errorContentStub.setLayoutResource(R.layout.plus_address_creation_error_state);
        errorContentStub.inflate();

        TextView title = mContentView.findViewById(R.id.plus_address_error_title);
        TextView description = mContentView.findViewById(R.id.plus_address_error_description);
        Button okButton = mContentView.findViewById(R.id.plus_address_error_ok_button);
        Button cancelButton = mContentView.findViewById(R.id.plus_address_error_cancel_button);

        title.setText(errorStateInfo.getTitle());
        description.setText(errorStateInfo.getDescription());
        okButton.setText(errorStateInfo.getOkText());
        cancelButton.setText(errorStateInfo.getCancelText());
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

    private void maybeShowFirstTimeUseNotice(String plusAddressNotice, GURL learnMoreUrl) {
        TextView firstTimeNotice =
                mContentView.findViewById(R.id.plus_address_first_time_use_notice);
        if (plusAddressNotice.isEmpty()) {
            firstTimeNotice.setVisibility(View.GONE);
            return;
        }
        NoUnderlineClickableSpan settingsLink =
                new NoUnderlineClickableSpan(
                        mContext,
                        v -> {
                            mDelegate.openUrl(learnMoreUrl);
                        });
        SpannableString spannableString =
                SpanApplier.applySpans(
                        plusAddressNotice,
                        new SpanApplier.SpanInfo("<link>", "</link>", settingsLink));
        firstTimeNotice.setText(spannableString);
        firstTimeNotice.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
