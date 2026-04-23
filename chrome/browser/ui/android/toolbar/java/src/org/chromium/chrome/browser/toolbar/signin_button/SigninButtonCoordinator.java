// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.isToolbarTabletResizeRefactorEnabled;

import android.content.Context;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarChildButton;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator for a signin button on the NTP toolbar. Owns the SigninButton view. */
@NullMarked
public class SigninButtonCoordinator extends ToolbarChildButton implements UrlFocusChangeListener {
    private final Context mContext;
    private final SigninButtonMediator mMediator;
    private final PropertyModel mModel;
    private final @Nullable ViewStub mViewStub;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final Runnable mTransitionTrigger;
    private @Nullable SigninButtonView mView;
    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private @Nullable OmniboxStub mOmniboxStub;
    private boolean mUrlHasFocus;

    public SigninButtonCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ViewStub viewStub,
            NullableObservableSupplier<Tab> tabSupplier,
            OneshotSupplier<OmniboxStub> omniboxStubSupplier,
            Runnable transitionTrigger,
            MonotonicObservableSupplier<Profile> profileSupplier,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider) {
        super(context, themeColorProvider, incognitoStateProvider);
        mContext = context;
        mTabSupplier = tabSupplier;
        mTransitionTrigger = transitionTrigger;
        mModel = new PropertyModel.Builder(SigninButtonProperties.ALL_KEYS).build();
        mMediator =
                new SigninButtonMediator(
                        context,
                        windowAndroid,
                        mModel,
                        profileSupplier,
                        signinAndHistorySyncActivityLauncher,
                        activityResultTracker,
                        deviceLockActivityLauncher,
                        bottomSheetController,
                        modalDialogManager,
                        snackbarManager,
                        themeColorProvider);

        // Defers setting the view and binding the model until the button needs to be shown.
        mViewStub = viewStub;

        omniboxStubSupplier.onAvailable(this::onOmniboxStubAvailable);
    }

    private void onOmniboxStubAvailable(OmniboxStub omniboxStub) {
        mOmniboxStub = omniboxStub;
        mUrlHasFocus = mOmniboxStub.isUrlBarFocused();
        mOmniboxStub.addUrlFocusChangeListener(this);
        updateButtonVisibility();
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
        updateButtonVisibility();
    }

    /**
     * Sets whether the SigninButton has space to show and inflates SigninButton view if needed.
     *
     * @param hasSpaceToShow Whether the button has space to show.
     */
    @Override
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        mMediator.setHasSpaceToShow(hasSpaceToShow);
        maybeInflateView();
    }

    /**
     * @return Returns true if the SigninButton view is attached with its visibility set to VISIBLE.
     */
    @Override
    public boolean isVisible() {
        return mView != null && mView.getVisibility() == View.VISIBLE;
    }

    /**
     * Updates whether the button has space to show based on available width.
     *
     * @param availableWidth The width available for this button.
     * @return The width used by the button.
     */
    @Override
    public int updateVisibility(int availableWidth) {
        assert isToolbarTabletResizeRefactorEnabled();

        // Cannot measure accurately if inflation has not happened yet.
        maybeInflateView();

        if (mView == null) {
            return 0;
        }

        final int width;
        if (mModel.get(SigninButtonProperties.USE_SIGNIN_TEXT_BUTTON)) {
            mView.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            width = mView.getMeasuredWidth();
        } else {
            width = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        }
        setHasSpaceToShow(availableWidth >= width);
        return Math.min(availableWidth, width);
    }

    /**
     * Called by the toolbar to set whether the button should be shown based on page state (e.g.
     * only on the NTP) and inflates SigninButton view if needed.
     */
    public void updateButtonVisibility() {
        Tab tab = mTabSupplier.get();
        // Should only show the signin button when on the NTP and not incognito.
        // It should also be hidden when the url bar has focus on a phone-like UI.
        boolean shouldHideButtonForUrlFocus =
                !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext) && mUrlHasFocus;
        boolean showSigninButtonOnNtp =
                tab != null
                        && UrlUtilities.isNtpUrl(tab.getUrl())
                        && !tab.isOffTheRecord()
                        && !shouldHideButtonForUrlFocus;

        // Only update signin button if it does not match the intended state.
        if (showSigninButtonOnNtp != isShown()) {
            mTransitionTrigger.run();
            mMediator.updateButtonVisibility(showSigninButtonOnNtp);
            maybeInflateView();
        }
    }

    private void maybeInflateView() {
        if (mModel.get(SigninButtonProperties.SHOULD_SHOW_ON_PAGE)
                && mView == null
                && mViewStub != null) {

            // Once the view initially is set to be visible, SigninButtonView should be inflated.
            mView = (SigninButtonView) mViewStub.inflate();
            mPropertyModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel, mView, SigninButtonViewBinder::bind);
        }
    }

    /** Returns whether the signin button should be visible. */
    public boolean isShown() {
        return mModel.get(SigninButtonProperties.SHOULD_SHOW_ON_PAGE);
    }

    /** Returns the signin button view for drawing. */
    public @Nullable View getViewForDrawing() {
        return mView;
    }

    /** Call to tear down dependencies. */
    @Override
    public void destroy() {
        if (mOmniboxStub != null) {
            mOmniboxStub.removeUrlFocusChangeListener(this);
        }
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        mMediator.destroy();
    }
}
