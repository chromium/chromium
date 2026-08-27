// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.method.LinkMovementMethod;
import android.view.FocusFinder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.containment.ContainerStyle;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.ui.widget.TextViewWithLeading;

/**
 * The view for the enterprise signals disclaimer. Inflates the layout and manages references to
 * child views.
 */
@NullMarked
class EnterpriseSignalsDisclaimerView extends FrameLayout {
    private final ScrollView mScrollView;
    private final ImageView mDisclaimerLogo;
    private final TextView mTitleView;
    private final TextViewWithClickableSpans mDescriptionView;
    private final TextView mProfileInformationTitle;
    private final TextViewWithLeading mProfileInformationDetails;
    private final TextView mDeviceInformationTitle;
    private final TextViewWithLeading mDeviceInformationDetails;
    private final ButtonCompat mAcceptButton;
    private final ButtonCompat mCancelButton;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerView}.
     *
     * @param context The Android {@link Context}.
     * @param isDialog Whether this view is shown inside a dialog.
     */
    protected EnterpriseSignalsDisclaimerView(Context context, boolean isDialog) {
        super(context);
        LayoutInflater.from(context)
                .inflate(R.layout.enterprise_signals_disclaimer_layout, this, true);

        mScrollView = findViewById(R.id.disclaimer_scroll_view);
        mDisclaimerLogo = findViewById(R.id.disclaimer_logo);
        mTitleView = findViewById(R.id.disclaimer_title);
        mDescriptionView = findViewById(R.id.disclaimer_description);
        mProfileInformationTitle = findViewById(R.id.profile_information_title);
        mProfileInformationDetails = findViewById(R.id.profile_information_details);
        mDeviceInformationTitle = findViewById(R.id.device_information_title);
        mDeviceInformationDetails = findViewById(R.id.device_information_details);

        ViewGroup buttonsContainer = findViewById(R.id.disclaimer_buttons_container);
        LayoutInflater.from(context)
                .inflate(
                        isDialog
                                ? R.layout.enterprise_signals_disclaimer_buttons_horizontal
                                : R.layout.enterprise_signals_disclaimer_buttons_vertical,
                        buttonsContainer,
                        true);

        mAcceptButton = findViewById(R.id.disclaimer_accept_button);
        mCancelButton = findViewById(R.id.disclaimer_cancel_button);

        ContainmentItemController controller = new ContainmentItemController(context);
        styleContainmentCard(
                controller, R.id.profile_info_card, /* isTop= */ true, /* isBottom= */ false);
        styleContainmentCard(
                controller, R.id.device_info_card, /* isTop= */ false, /* isBottom= */ true);

        mDescriptionView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Creates an {@link EnterpriseSignalsDisclaimerView} configured for a modal dialog.
     *
     * @param context The Android {@link Context}.
     */
    public static EnterpriseSignalsDisclaimerView createForModalDialog(Context context) {
        return new EnterpriseSignalsDisclaimerView(context, /* isDialog= */ true);
    }

    /** Returns the scroll Y offset. */
    public int getScrollViewScrollY() {
        return mScrollView.getScrollY();
    }

    /**
     * Sets the profile picture displayed in the disclaimer.
     *
     * @param profilePicture The {@link Drawable} for the profile picture.
     */
    public void setProfilePicture(@Nullable Drawable profilePicture) {
        if (profilePicture != null) {
            mDisclaimerLogo.setImageDrawable(profilePicture);
        }
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
    public void setDescription(CharSequence description) {
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

    /**
     * Confines the focus search to the dialog. This prevents accidental dismissal by holding tab or
     * arrows. If the last element is selected, the selection will loop back.
     */
    @Override
    public View focusSearch(View focused, int direction) {
        View nextCandidate = FocusFinder.getInstance().findNextFocus(this, focused, direction);
        if (nextCandidate != null) {
            return nextCandidate;
        } else {
            return FocusFinder.getInstance().findNextFocus(this, null, direction);
        }
    }

    private void styleContainmentCard(
            ContainmentItemController controller, int viewId, boolean isTop, boolean isBottom) {
        View view = findViewById(viewId);
        ContainerStyle.Builder builder =
                controller.createStandardBuilder(isTop, isBottom, /* isSingleLine= */ false);
        if (view instanceof ContainmentItem containmentItem) {
            controller.addCustomStyling(builder, containmentItem);
        }
        ContainmentViewStyler.applyBackgroundStyle(view, builder.build());
    }
}
