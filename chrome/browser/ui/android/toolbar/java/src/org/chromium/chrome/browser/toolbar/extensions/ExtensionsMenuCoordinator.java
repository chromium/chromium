// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Coordinator for the extensions menu, access from the puzzle icon in the toolbar. */
@NullMarked
public class ExtensionsMenuCoordinator implements Destroyable {
    private final Context mContext;
    private final ListMenuButton mExtensionsMenuButton;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private final TabCreator mTabCreator;
    private final AnchoredPopupWindow mMenuWindow;
    private final View mContentView;
    private final PropertyModelChangeProcessor mChangeProcessor;

    /**
     * Constructor.
     *
     * @param context The context for this component.
     * @param extensionsMenuButton The puzzle icon in the toolbar.
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     */
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator) {
        this(context, extensionsMenuButton, currentTabSupplier, tabCreator, null);
    }

    @VisibleForTesting
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            @Nullable AnchoredPopupWindow menuWindow) {
        mContext = context;
        mExtensionsMenuButton = extensionsMenuButton;

        mCurrentTabSupplier = currentTabSupplier;

        mTabCreator = tabCreator;

        View decorView = ((Activity) mContext).getWindow().getDecorView();
        ViewRectProvider anchoredViewRectProvider = new ViewRectProvider(mExtensionsMenuButton);
        mContentView = LayoutInflater.from(mContext).inflate(R.layout.extensions_menu, null, false);

        if (menuWindow != null) {
            mMenuWindow = menuWindow;
        } else {
            mMenuWindow =
                    new AnchoredPopupWindow(
                            mContext,
                            decorView,
                            AppCompatResources.getDrawable(
                                    mContext, R.drawable.extensions_menu_bg_tinted),
                            mContentView,
                            anchoredViewRectProvider);
        }

        PropertyModel model =
                new PropertyModel.Builder(ExtensionsMenuProperties.ALL_KEYS)
                        .with(
                                ExtensionsMenuProperties.CLOSE_CLICK_LISTENER,
                                (view) -> {
                                    closeMenu();
                                })
                        .with(
                                ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER,
                                (view) -> {
                                    openUrlFromMenu(UrlConstants.CHROME_WEBSTORE_URL);
                                })
                        .with(
                                ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER,
                                (view) -> {
                                    openUrlFromMenu(UrlConstants.CHROME_EXTENSIONS_URL);
                                })
                        .build();

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mContentView, ExtensionsMenuViewBinder::bind);
    }

    /** Shows the extensions menu. */
    public void showMenu() {
        mMenuWindow.show();
    }

    private void openUrlFromMenu(String url) {
        closeMenu();

        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);

        // We want to open the URL in the current tab if possible to match the behaviors of other
        // menu options (e.g. history).
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) {
            mTabCreator.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
        } else {
            currentTab.loadUrl(params);
        }
    }

    private void closeMenu() {
        mMenuWindow.dismiss();
    }

    @Override
    public void destroy() {
        mChangeProcessor.destroy();
    }

    @VisibleForTesting
    View getContentView() {
        return mContentView;
    }
}
