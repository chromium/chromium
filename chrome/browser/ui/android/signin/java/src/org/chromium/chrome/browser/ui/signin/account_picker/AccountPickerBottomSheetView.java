// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Space;
import android.widget.TextView;
import android.widget.ViewFlipper;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithLeading;

/**
 * This class is the AccountPickerBottomsheet view for the web sign-in flow.
 *
 * The bottom sheet shows a single account with a |Continue as ...| button by default, clicking
 * on the account will expand the bottom sheet to an account list together with other sign-in
 * options like "Add account".
 */
class AccountPickerBottomSheetView implements BottomSheetContent {
    /** Listener for the back-press button. */
    interface BackPressListener {
        /**
         * Notifies when user clicks the back-press button.
         * @return true if the listener handles the back press, false if not.
         */
        boolean onBackPressed();

        /**
         * @return A supplier that determines if back press will be handled by the sheet content.
         */
        default ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
            ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
            supplier.set(false);
            return supplier;
        }
    }

    /**
     * The title id for each screen of the bottom sheet's view flipper, the position of each id
     * corresponds to the value of {@link ViewState}. It is used to set focus on title when the view
     * flipper moves to a new screen.
     */
    private static final @IdRes int[] sTitleIds =
            new int[] {
                R.id.account_picker_header_title,
                R.id.account_picker_header_title,
                R.id.account_picker_header_title,
                R.id.account_picker_signin_in_progress_title,
                R.id.account_picker_general_error_title,
                R.id.account_picker_auth_error_title,
                R.id.account_picker_confirm_management_title,
            };

    private final Activity mActivity;
    private final BackPressListener mBackPressListener;
    private final View mContentView;
    private final ViewFlipper mViewFlipper;
    private final RecyclerView mAccountListView;
    private final View mSelectedAccountView;
    private final ButtonCompat mDismissButton;
    private final Space mDismissButtonGoneMarginSpace;
    private @Nullable @ViewState Integer mCurrentViewState;

    /**
     * @param activity The activity that hosts this view. Used for inflating views.
     * @param backPressListener The listener to be notified when the user taps the back button.
     */
    AccountPickerBottomSheetView(Activity activity, BackPressListener backPressListener) {
        mActivity = activity;
        mBackPressListener = backPressListener;

        int contentLayoutId =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? R.layout.account_picker_bottom_sheet_view
                        : R.layout.account_picker_bottom_sheet_view_old;

        mContentView = LayoutInflater.from(mActivity).inflate(contentLayoutId, null);

        mViewFlipper = mContentView.findViewById(R.id.account_picker_state_view_flipper);
        checkViewFlipperChildrenAndViewStateMatch(mViewFlipper);
        mAccountListView =
                mViewFlipper
                        .getChildAt(ViewState.EXPANDED_ACCOUNT_LIST)
                        .findViewById(R.id.account_picker_account_list);
        mAccountListView.setLayoutManager(
                new LinearLayoutManager(
                        mAccountListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSelectedAccountView =
                mViewFlipper
                        .getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST)
                        .findViewById(R.id.account_picker_selected_account);
        mDismissButton =
                mViewFlipper
                        .getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST)
                        .findViewById(R.id.account_picker_dismiss_button);
        mDismissButtonGoneMarginSpace =
                mViewFlipper
                        .getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST)
                        .findViewById(R.id.account_picker_dismiss_button_gone_margin_space);

        setUpContinueButton(
                mViewFlipper.getChildAt(ViewState.NO_ACCOUNTS),
                R.string.signin_add_account_to_device);
        setUpContinueButton(
                mViewFlipper.getChildAt(ViewState.CONFIRM_MANAGEMENT), R.string.continue_button);
        setUpContinueButton(
                mViewFlipper.getChildAt(ViewState.SIGNIN_GENERAL_ERROR),
                R.string.signin_account_picker_general_error_button);
        setUpContinueButton(
                mViewFlipper.getChildAt(ViewState.SIGNIN_AUTH_ERROR),
                R.string.auth_error_card_button);

        mViewFlipper
                .getChildAt(ViewState.CONFIRM_MANAGEMENT)
                .findViewById(R.id.confirm_management_cancel_button)
                .setOnClickListener((View v) -> handleBackPress());

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            getAccountListView().addItemDecoration(new AccountPickerItemDecoration());
        }
    }

    /** The account list view is visible when the account list is expanded. */
    RecyclerView getAccountListView() {
        return mAccountListView;
    }

    /** The selected account is visible when the account list is collapsed. */
    View getSelectedAccountView() {
        return mSelectedAccountView;
    }

    /** Sets the listener of the continue button. */
    void setOnClickListenerOfContinueButton(OnClickListener listener) {
        for (int i = 0; i < mViewFlipper.getChildCount(); ++i) {
            ButtonCompat continueButton =
                    mViewFlipper.getChildAt(i).findViewById(R.id.account_picker_continue_as_button);
            if (continueButton != null) {
                continueButton.setOnClickListener(listener);
            }
        }
    }

    /** The button to dismiss the bottom sheet. */
    ButtonCompat getDismissButton() {
        return mDismissButton;
    }

    /** Sets the displayed view according to the given {@link ViewState}. */
    void setDisplayedView(@ViewState int state) {
        if (mCurrentViewState != null && mCurrentViewState == state) {
            return;
        }

        mViewFlipper.setDisplayedChild(state);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                && state == ViewState.SIGNIN_IN_PROGRESS
                && mCurrentViewState != null) {
            // The goal here is to make the progress view take the height of the previously shown
            // view, to prevent the bottom sheet from "jumping" visually.
            // (See https://crbug.com/327127097)
            //
            // At this point, all other children of mViewFlipper are set to `GONE` by
            // `mViewFlipper.setDisplayedChild` above.
            // Here, the previous view's visibility is set to `INVISIBLE` instead. Since the
            // progress view's height is `MATCH_PARENT` and it's minimal height is smaller,
            // the bottom sheet view's height will be defined by the invisible previous view, making
            // the progress view having the same height than the previous view in consequence.
            mViewFlipper.getChildAt(mCurrentViewState).setVisibility(View.INVISIBLE);
        }
        mCurrentViewState = state;
        View titleView = mViewFlipper.getChildAt(state).findViewById(sTitleIds[state]);
        titleView.setFocusable(true);
        titleView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /**
     * Updates the views related to the selected account.
     *
     * <p>This method only updates the UI elements like text related to the selected account, it
     * does not change the visibility.
     */
    void updateSelectedAccount(DisplayableProfileData accountProfileData) {
        View view = mViewFlipper.getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST);
        ExistingAccountRowViewBinder.bindAccountView(
                accountProfileData, mSelectedAccountView, /* isCurrentlySelected= */ true);
        ButtonCompat continueButton = view.findViewById(R.id.account_picker_continue_as_button);
        continueButton.setText(
                SigninUtils.getContinueAsButtonText(view.getContext(), accountProfileData));
    }

    void updateSelectedDomain(String domain) {
        TextViewWithLeading confirmManagementDescription =
                mViewFlipper.findViewById(R.id.account_picker_confirm_management_description);
        confirmManagementDescription.setText(
                mActivity.getString(R.string.managed_signin_with_user_policy_subtitle, domain));
    }

    /** Sets the title, subtitle, and dismiss button text. */
    void setBottomSheetStrings(
            @StringRes int title, @StringRes int subtitle, @StringRes int cancelButton) {
        final int[] viewStates = {
            ViewState.COLLAPSED_ACCOUNT_LIST, ViewState.EXPANDED_ACCOUNT_LIST, ViewState.NO_ACCOUNTS
        };
        for (int viewState : viewStates) {
            final View view = mViewFlipper.getChildAt(viewState);
            ((TextView) view.findViewById(R.id.account_picker_header_title)).setText(title);

            TextViewWithLeading subtitleView =
                    view.findViewById(R.id.account_picker_header_subtitle);
            if (subtitle == 0) {
                subtitleView.setVisibility(View.GONE);
            } else {
                subtitleView.setText(subtitle);
                subtitleView.setVisibility(View.VISIBLE);
            }
        }

        if (cancelButton == 0) {
            showDismissButton(false);
        } else {
            mDismissButton.setText(cancelButton);
            showDismissButton(true);
        }
    }

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
        return 0;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean handleBackPress() {
        return mBackPressListener.onBackPressed();
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressListener.getBackPressStateChangedSupplier();
    }

    @Override
    public void onBackPressed() {
        mBackPressListener.onBackPressed();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.signin_account_picker_bottom_sheet_subtitle;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.account_picker_bottom_sheet_accessibility_opened;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.account_picker_bottom_sheet_accessibility_opened;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.account_picker_bottom_sheet_accessibility_closed;
    }

    @Override
    public boolean shouldLongPressMoveSheet() {
        return true;
    }

    private static void setUpContinueButton(View view, @StringRes int buttonId) {
        ButtonCompat continueButton = view.findViewById(R.id.account_picker_continue_as_button);
        continueButton.setText(buttonId);
    }

    private static void checkViewFlipperChildrenAndViewStateMatch(ViewFlipper viewFlipper) {
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.NO_ACCOUNTS, R.id.account_picker_state_no_account);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.COLLAPSED_ACCOUNT_LIST, R.id.account_picker_state_collapsed);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.EXPANDED_ACCOUNT_LIST, R.id.account_picker_state_expanded);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper,
                ViewState.SIGNIN_IN_PROGRESS,
                R.id.account_picker_state_signin_in_progress);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper,
                ViewState.SIGNIN_GENERAL_ERROR,
                R.id.account_picker_state_general_error);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.SIGNIN_AUTH_ERROR, R.id.account_picker_state_auth_error);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper,
                ViewState.CONFIRM_MANAGEMENT,
                R.id.account_picker_state_confirm_management);
    }

    private static void checkViewFlipperChildIdAndViewStateMatch(
            ViewFlipper viewFlipper, @ViewState int viewState, @IdRes int expectedChildId) {
        if (viewFlipper.getChildAt(viewState).getId() != expectedChildId) {
            throw new IllegalArgumentException("Match failed with ViewState:" + viewState);
        }
    }

    private void showDismissButton(boolean shouldShow) {
        if (shouldShow) {
            mDismissButton.setVisibility(View.VISIBLE);
            mDismissButtonGoneMarginSpace.setVisibility(View.GONE);
        } else {
            mDismissButton.setVisibility(View.GONE);
            mDismissButtonGoneMarginSpace.setVisibility(View.VISIBLE);
        }
    }
}
