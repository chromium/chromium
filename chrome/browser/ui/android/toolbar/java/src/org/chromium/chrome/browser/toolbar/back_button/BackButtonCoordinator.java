// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.content.res.ColorStateList;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Root component for the back button. Exposes public API for external consumers to interact with
 * the button and affect its state.
 */
@NullMarked
public class BackButtonCoordinator {
    private final BackButtonMediator mMediator;
    private final NavigationPopup.HistoryDelegate mHistoryDelegate;
    private final Supplier<Tab> mTabSupplier;
    private final View mView;

    /**
     * Creates an instance of {@link BackButtonCoordinator}.
     *
     * @param view an Android {@link ImageButton}.
     * @param onBackPressed a callback that is invoked on back button click event. Allows parent
     *     components to intercept click and navigate back in the history or hide custom UI
     *     components.
     * @param themeColorProvider a provider that notifies about theme changes.
     * @param tabSupplier a supplier that provides current active tab.
     * @param historyDelegate a delegate that allows parent components to decide how to display
     *     browser history.
     */
    public BackButtonCoordinator(
            ImageButton view,
            Runnable onBackPressed,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<Tab> tabSupplier,
            NavigationPopup.HistoryDelegate historyDelegate) {
        mView = view;
        mTabSupplier = tabSupplier;
        mHistoryDelegate = historyDelegate;

        final ColorStateList iconColorList =
                themeColorProvider.getActivityFocusTint() == null
                        ? view.getImageTintList()
                        : themeColorProvider.getActivityFocusTint();
        final var model =
                new PropertyModel.Builder(BackButtonProperties.ALL_KEYS)
                        .with(BackButtonProperties.TINT_COLOR_LIST, iconColorList)
                        .build();
        mMediator =
                new BackButtonMediator(
                        model,
                        onBackPressed,
                        themeColorProvider,
                        tabSupplier,
                        this::showNavigationPopup);
        PropertyModelChangeProcessor.create(model, view, BackButtonViewBinder::bind);
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
    }

    /**
     * Cleans up coordinator resources and unsubscribes from external events. An instance can't be
     * used after this method is called.
     */
    public void destroy() {
        mMediator.destroy();
    }
}
