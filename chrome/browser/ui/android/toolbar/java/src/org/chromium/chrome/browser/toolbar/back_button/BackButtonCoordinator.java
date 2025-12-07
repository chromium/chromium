// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;

import androidx.core.graphics.Insets;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.toolbar.top.ToolbarChildButton;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ClickWithMetaStateCallback;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.function.Supplier;

/**
 * Root component for the back button. Exposes public API for external consumers to interact with
 * the button and affect its state.
 */
@NullMarked
public class BackButtonCoordinator extends ToolbarChildButton {
    private final BackButtonMediator mMediator;
    private final NavigationPopup.HistoryDelegate mHistoryDelegate;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final Runnable mOnNavigationPopupShown;
    private final View mView;

    /**
     * Creates an instance of {@link BackButtonCoordinator}.
     *
     * @param view an Android {@link ChromeImageButton}.
     * @param onBackPressed a {@link ClickWithMetaStateCallback} (taking a parameter of meta key
     *     state) that is invoked on back button click event. Allows parent components to intercept
     *     click and navigate back in the history or hide custom UI components.
     * @param themeColorProvider a provider that notifies about theme changes.
     * @param incognitoStateProvider a provider that notifies about incognito state changes.
     * @param tabSupplier a supplier that provides current active tab.
     * @param historyDelegate a delegate that allows parent components to decide how to display
     *     browser history.
     */
    public BackButtonCoordinator(
            ChromeImageButton view,
            ClickWithMetaStateCallback onBackPressed,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ObservableSupplier<Boolean> enabledSupplier,
            Runnable onNavigationPopupShown,
            NavigationPopup.HistoryDelegate historyDelegate,
            boolean isWebApp) {
        super(view.getContext(), themeColorProvider, incognitoStateProvider);
        mView = view;
        mTabSupplier = tabSupplier;
        mHistoryDelegate = historyDelegate;
        mOnNavigationPopupShown = onNavigationPopupShown;

        final ColorStateList iconColorList =
                themeColorProvider.getActivityFocusTint() == null
                        ? view.getImageTintList()
                        : themeColorProvider.getActivityFocusTint();
        final var model =
                new PropertyModel.Builder(BackButtonProperties.ALL_KEYS)
                        .with(BackButtonProperties.TINT_COLOR_LIST, iconColorList)
                        .with(BackButtonProperties.IS_ENABLED, view.isEnabled())
                        .with(BackButtonProperties.IS_FOCUSABLE, view.isFocusable())
                        .with(
                                BackButtonProperties.IS_VISIBLE,
                                mView.getVisibility() == View.VISIBLE)
                        .with(BackButtonProperties.HAS_SPACE_TO_SHOW, true)
                        .build();
        mMediator =
                new BackButtonMediator(
                        model,
                        onBackPressed,
                        themeColorProvider,
                        tabSupplier,
                        enabledSupplier,
                        this::showNavigationPopup,
                        mView.getResources(),
                        mView.getContext(),
                        isWebApp);
        PropertyModelChangeProcessor.create(model, view, BackButtonViewBinder::bind);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        super.onTintChanged(tint, activityFocusTint, brandedColorScheme);
        mMediator.onTintChanged(tint, activityFocusTint, brandedColorScheme);
    }

    private void showNavigationPopup(Tab tab) {
        if (tab.getWebContents() == null) return;

        final var popup =
                new NavigationPopup(
                        tab.getProfile(),
                        mView.getContext(),
                        tab.getWebContents().getNavigationController(),
                        NavigationPopup.Type.TABLET_BACK,
                        mTabSupplier,
                        mHistoryDelegate);
        popup.show(mView);
        mOnNavigationPopupShown.run();
    }

    /**
     * Prepares the view for fade animation and returns an alpha animator.
     *
     * @param shouldShow indicated fade in or out animation type
     * @return {@link ObjectAnimator} that animates view's alpha
     */
    public ObjectAnimator getFadeAnimator(boolean shouldShow) {
        ObjectAnimator fadeAnimator = mMediator.getFadeAnimator(shouldShow);
        return shouldShow
                ? ToolbarUtils.asFadeInAnimation(fadeAnimator)
                : ToolbarUtils.asFadeOutAnimation(fadeAnimator);
    }

    /**
     * Sets back button visibility.
     *
     * @param isVisible indicated whether view should be visible or gone.
     */
    public void setVisibility(boolean isVisible) {
        mMediator.setVisibility(isVisible);
    }

    @Override
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        mMediator.setHasSpaceToShow(hasSpaceToShow);
    }

    /**
     * Checks whether view is focusable or not.
     *
     * @return true - view is focusable, false - view is not focusable.
     */
    public boolean isFocusable() {
        return mMediator.isFocusable();
    }

    /**
     * Requests current view to gain focus.
     *
     * @return true - if view successfully gained focus, false - if view failed to gain focus.
     */
    public boolean focus() {
        return mView.requestFocus();
    }

    /**
     * Sets a key event listener on back button.
     *
     * @param listener {@link View.OnKeyListener}
     */
    public void setOnKeyListener(View.OnKeyListener listener) {
        mMediator.setOnKeyListener(listener);
    }

    public void setBackgroundInsets(Insets insets) {
        mMediator.setBackgroundInsets(insets);
    }

    /**
     * Gets an area of the button that are touchable/clickable.
     *
     * @return a {@link Rect} that contains touchable/clickable area.
     */
    public Rect getHitRect() {
        final var rect = new Rect();
        mView.getHitRect(rect);
        return rect;
    }

    /**
     * Gets visibility.
     *
     * @return a boolean indicating whether view is visible or not.
     */
    @Override
    public boolean isVisible() {
        return mMediator.isVisible();
    }

    /**
     * Cleans up coordinator resources and unsubscribes from external events. An instance can't be
     * used after this method is called.
     */
    @Override
    public void destroy() {
        super.destroy();
        mMediator.destroy();
    }
}
