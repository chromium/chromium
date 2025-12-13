// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.forward_button;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.view.KeyEvent;
import android.view.View;

import androidx.core.widget.ImageViewCompat;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.toolbar.top.ToolbarChildButton;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.function.Supplier;

/**
 * Root component for the back button. Exposes public API for external consumers to interact with
 * the button and affect its state.
 */
@NullMarked
public class ForwardButtonCoordinator extends ToolbarChildButton {
    private final Context mContext;
    private final ToolbarDataProvider mToolbarDataProvider;
    private final ToolbarTabController mToolbarTabController;
    private final Supplier<LocationBar> mLocationBarSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ChromeImageButton mImageButton;
    private final NavigationPopup.HistoryDelegate mHistoryDelegate;

    private @Nullable NavigationPopup mNavigationPopup;

    /**
     * Creates a ForwardButtonCoordinator for managing the forward button on the tablet toolbar.
     *
     * @param context The context in which the forward button is created.
     * @param toolbarDataProvider The provider for toolbar data.
     * @param toolbarTabController The controller that handles interactions with the tab.
     * @param locationBarSupplier Supplies the {@link LocationBar} for unfocusing the location when
     *     moving the tab state forward.
     * @param activityLifecycleDispatcher Informs the activity lifecycle state for checking whether
     *     native has been initialized.
     * @param imageButton The image button for the forward button.
     * @param historyDelegate The delegate for history-related actions.
     * @param themeColorProvider Provides theme and tint color that should be applied to the view.
     * @param incognitoStateProvider Provides incognito state to update view.
     */
    public ForwardButtonCoordinator(
            Context context,
            ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController toolbarTabController,
            Supplier<LocationBar> locationBarSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ChromeImageButton imageButton,
            NavigationPopup.HistoryDelegate historyDelegate,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider) {
        super(context, themeColorProvider, incognitoStateProvider);
        mContext = context;
        mToolbarDataProvider = toolbarDataProvider;
        mToolbarTabController = toolbarTabController;
        mLocationBarSupplier = locationBarSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;

        mImageButton = imageButton;
        mHistoryDelegate = historyDelegate;

        onTintChanged(
                mTopUiThemeColorProvider.getTint(),
                mTopUiThemeColorProvider.getActivityFocusTint(),
                mTopUiThemeColorProvider.getBrandedColorScheme());
        onIncognitoStateChanged(mIncognitoStateProvider.isIncognitoSelected());

        mImageButton.setClickCallback(metaState -> forward(metaState, "MobileToolbarForward"));
        mImageButton.setLongClickable(true);
    }

    /** Returns the forward button view. */
    public ChromeImageButton getButton() {
        return mImageButton;
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        super.onTintChanged(tint, activityFocusTint, brandedColorScheme);
        if (mImageButton != null) {
            ImageViewCompat.setImageTintList(mImageButton, activityFocusTint);
        }
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        super.onIncognitoStateChanged(isIncognito);
        if (mImageButton != null) {
            mImageButton.setBackgroundResource(ToolbarUtils.getToolbarIconRippleId(isIncognito));
        }
    }

    @Override
    public boolean isVisible() {
        return mImageButton.getVisibility() == View.VISIBLE;
    }

    // TODO(crbug.com/455658153): Ensure setVisibility() can handle multiple sources for setting
    //  visibility. Currently this only accounts for visibility being set due to the width of the
    //  ToolbarTablet.
    public void setVisibility(boolean visibility) {
        mImageButton.setVisibility(visibility ? View.VISIBLE : View.GONE);
    }

    @Override
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        setVisibility(hasSpaceToShow);
    }

    /** Updates whether the forward button is enabled based on the current Tab. */
    public void updateEnabled() {
        Tab tab = mToolbarDataProvider.getTab();
        boolean enabled = tab != null && tab.canGoForward();
        mImageButton.setEnabled(enabled);
        mImageButton.setFocusable(enabled);
    }

    /** Called when the window containing the forward button gains or loses focus. */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        // Ensure the the popup is not shown after resuming activity from background.
        if (hasWindowFocus && mNavigationPopup != null) {
            mNavigationPopup.dismiss();
            mNavigationPopup = null;
        }
    }

    /** Displays the navigation popup for the forward button. */
    public void displayNavigationPopup() {
        Tab tab = mToolbarDataProvider.getTab();
        if (tab == null || tab.getWebContents() == null) return;
        mNavigationPopup =
                new NavigationPopup(
                        tab.getProfile(),
                        mContext,
                        tab.getWebContents().getNavigationController(),
                        NavigationPopup.Type.TABLET_FORWARD,
                        mToolbarDataProvider::getTab,
                        mHistoryDelegate);
        mNavigationPopup.show(mImageButton);
    }

    /**
     * Navigates the current Tab forward.
     *
     * @return Whether or not the current Tab did go forward.
     */
    protected boolean forward(int metaState, String reportingTagPrefix) {
        if (!mActivityLifecycleDispatcher.isNativeInitializationFinished()) return false;

        maybeUnfocusUrlBar();
        boolean hasControl = (metaState & KeyEvent.META_CTRL_ON) != 0;
        boolean hasShift = (metaState & KeyEvent.META_SHIFT_ON) != 0;
        if (hasControl && hasShift) {
            // Holding ALT is allowed as well (reference desktop behavior).

            // Note on recording user actions: "forward" is recorded regardless of whether it
            // was successful. See
            // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarTablet.java;l=196;drc=14aab80e079b7db3e85a8302da6d660bafeddfbc
            RecordUserAction.record(reportingTagPrefix + "InNewForegroundTab");
            return mToolbarTabController.forwardInNewTab(/* foregroundNewTab= */ true);
        } else if (hasControl) {
            RecordUserAction.record(reportingTagPrefix + "InNewBackgroundTab");
            return mToolbarTabController.forwardInNewTab(/* foregroundNewTab= */ false);
        } else if (hasShift) {
            RecordUserAction.record(reportingTagPrefix + "InNewForegroundWindow");
            return mToolbarTabController.forwardInNewWindow();
        } else {
            RecordUserAction.record(reportingTagPrefix);
            return mToolbarTabController.forward();
        }
    }

    private void maybeUnfocusUrlBar() {
        LocationBar locationBar = mLocationBarSupplier.get();
        if (locationBar != null && locationBar.getOmniboxStub() != null) {
            locationBar
                    .getOmniboxStub()
                    .setUrlBarFocus(
                            false,
                            null,
                            OmniboxFocusReason.UNFOCUS,
                            AutocompleteRequestType.SEARCH);
        }
    }

    /**
     * Prepares the view for fade animation and returns an alpha animator.
     *
     * @param shouldShow indicated fade in or out animation type
     * @return {@link ObjectAnimator} that animates view's alpha
     */
    public ObjectAnimator getFadeAnimator(boolean shouldShow) {
        return shouldShow
                ? createShowButtonAnimatorForTablet()
                : createHideButtonAnimatorForTablet();
    }

    private ObjectAnimator createShowButtonAnimatorForTablet() {
        if (mImageButton.getVisibility() != View.VISIBLE) {
            mImageButton.setAlpha(0.f);
        }
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(mImageButton, View.ALPHA, 1.f);
        return ToolbarUtils.asFadeInAnimation(buttonAnimator);
    }

    private ObjectAnimator createHideButtonAnimatorForTablet() {
        if (mImageButton.getVisibility() != View.VISIBLE) {
            mImageButton.setAlpha(0.f);
        }
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(mImageButton, View.ALPHA, 0.f);
        return ToolbarUtils.asFadeOutAnimation(buttonAnimator);
    }
}
