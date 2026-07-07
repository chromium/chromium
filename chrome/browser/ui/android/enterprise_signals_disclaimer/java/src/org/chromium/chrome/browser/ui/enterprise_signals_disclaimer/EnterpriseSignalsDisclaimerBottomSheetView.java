// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.widget.containment.ContainerStyle;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithLeading;

/**
 * The bottom sheet content view for the enterprise signals disclaimer. Inflates the layout and
 * manages references to child views.
 */
@NullMarked
class EnterpriseSignalsDisclaimerBottomSheetView implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;
    private final TextView mTitleView;
    private final TextViewWithLeading mDescriptionView;
    private final TextView mProfileInformationTitle;
    private final TextViewWithLeading mProfileInformationDetails;
    private final TextView mDeviceInformationTitle;
    private final TextViewWithLeading mDeviceInformationDetails;
    private final ButtonCompat mAcceptButton;
    private final ButtonCompat mCancelButton;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerBottomSheetView}.
     *
     * @param context The Android {@link Context}.
     */
    public EnterpriseSignalsDisclaimerBottomSheetView(Context context) {
        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.enterprise_signals_disclaimer_layout, null);
        mScrollView = mContentView.findViewById(R.id.disclaimer_scroll_view);
        mTitleView = mContentView.findViewById(R.id.disclaimer_title);
        mDescriptionView = mContentView.findViewById(R.id.disclaimer_description);
        mProfileInformationTitle = mContentView.findViewById(R.id.profile_information_title);
        mProfileInformationDetails = mContentView.findViewById(R.id.profile_information_details);
        mDeviceInformationTitle = mContentView.findViewById(R.id.device_information_title);
        mDeviceInformationDetails = mContentView.findViewById(R.id.device_information_details);
        mAcceptButton = mContentView.findViewById(R.id.disclaimer_accept_button);
        mCancelButton = mContentView.findViewById(R.id.disclaimer_cancel_button);

        ContainmentItemController controller = new ContainmentItemController(context);
        styleContainmentCard(
                controller, R.id.profile_info_card, /* isTop= */ true, /* isBottom= */ false);
        styleContainmentCard(
                controller, R.id.device_info_card, /* isTop= */ false, /* isBottom= */ true);
    }

    /**
     * Sets the disclaimer title text.
     *
     * @param title The title text.
     */
    public void setTitle(String title) {
        mTitleView.setText(title);
    }

    /**
     * Sets the disclaimer description text.
     *
     * @param description The description text.
     */
    public void setDescription(String description) {
        mDescriptionView.setText(description);
    }

    /**
     * Sets the title text for the profile information section.
     *
     * @param text The section title text.
     */
    public void setProfileInformationTitle(String text) {
        mProfileInformationTitle.setText(text);
    }

    /**
     * Sets the details text for the profile information section.
     *
     * @param text The section details text.
     */
    public void setProfileInformationDetails(String text) {
        mProfileInformationDetails.setText(text);
    }

    /**
     * Sets the title text for the device information section.
     *
     * @param text The section title text.
     */
    public void setDeviceInformationTitle(String text) {
        mDeviceInformationTitle.setText(text);
    }

    /**
     * Sets the details text for the device information section.
     *
     * @param text The section details text.
     */
    public void setDeviceInformationDetails(String text) {
        mDeviceInformationDetails.setText(text);
    }

    /**
     * Sets the text displayed on the accept button.
     *
     * @param text The button text.
     */
    public void setAcceptButtonText(String text) {
        mAcceptButton.setText(text);
    }

    /**
     * Sets the text displayed on the cancel button.
     *
     * @param text The button text.
     */
    public void setCancelButtonText(String text) {
        mCancelButton.setText(text);
    }

    /**
     * Sets the click listener for the accept button.
     *
     * @param listener The click listener.
     */
    public void setOnAcceptClicked(OnClickListener listener) {
        mAcceptButton.setOnClickListener(listener);
    }

    /**
     * Sets the click listener for the cancel button.
     *
     * @param listener The click listener.
     */
    public void setOnCancelClicked(OnClickListener listener) {
        mCancelButton.setOnClickListener(listener);
    }

    // BottomSheetContent implementation:
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
        return mScrollView.getScrollY();
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        // TODO(b/512836948): Add a localised string once the dialog is complete.
        return "";
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // TODO(b/512836948): Add a localised string once the dialog is complete.
        return R.string.bottom_sheet_accessibility_description;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        // TODO(b/512836948): Add a localised string once the dialog is complete.
        return R.string.bottom_sheet_accessibility_description;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        // TODO(b/512836948): Add a localised string once the dialog is complete.
        return R.string.bottom_sheet_accessibility_description;
    }

    private void styleContainmentCard(
            ContainmentItemController controller, int viewId, boolean isTop, boolean isBottom) {
        View view = mContentView.findViewById(viewId);
        ContainerStyle.Builder builder =
                controller.createStandardBuilder(isTop, isBottom, /* isSingleLine= */ false);
        if (view instanceof ContainmentItem containmentItem) {
            controller.addCustomStyling(builder, containmentItem);
        }
        ContainmentViewStyler.applyBackgroundStyle(view, builder.build());
    }
}
