// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Coordinator for the extensions menu, access from the puzzle icon in the toolbar. */
@NullMarked
public class ExtensionsMenuCoordinator implements Destroyable {
    private final Context mContext;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private final TabCreator mTabCreator;
    private final AnchoredPopupWindow mMenuWindow;
    private final View mContentView;
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final ExtensionsMenuMediator mMediator;
    private final ModelList mExtensionModels;

    private boolean mShouldShowMenu;
    private boolean mExtensionDataReady;

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
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator) {
        this(context, extensionsMenuButton, profileSupplier, currentTabSupplier, tabCreator, null);
    }

    @VisibleForTesting
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            @Nullable AnchoredPopupWindow menuWindow) {
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mTabCreator = tabCreator;

        View decorView = ((Activity) mContext).getWindow().getDecorView();
        mContentView = LayoutInflater.from(mContext).inflate(R.layout.extensions_menu, null, false);

        PropertyModel model = createMenuPropertyModel();

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mContentView, ExtensionsMenuViewBinder::bind);

        mExtensionModels = new ModelList();
        mMediator =
                new ExtensionsMenuMediator(
                        profileSupplier,
                        currentTabSupplier,
                        mExtensionModels,
                        () -> {
                            mExtensionDataReady = true;
                            updateWindowVisibility();
                        });

        setUpExtensionsRecyclerView(mContentView, mContext, mExtensionModels);

        if (menuWindow != null) {
            mMenuWindow = menuWindow;
        } else {
            mMenuWindow =
                    createPopupWindow(mContext, decorView, extensionsMenuButton, mContentView);
        }
    }

    /** Show the extensions menu (potentially async). */
    public void showMenu() {
        mShouldShowMenu = true;
        // The mediator has to wait for `onProfileUpdated` on creation. We don't change window
        // visibility here directly because we want the menu to be shown only after the `Mediator`
        // notifies us that the data is ready.
        updateWindowVisibility();
    }

    private void closeMenu() {
        mShouldShowMenu = false;
        updateWindowVisibility();
    }

    private void updateWindowVisibility() {
        if (mShouldShowMenu && mExtensionDataReady) {
            // We have to make sure that the extension data created in the mediator is ready
            // before we can show the menu window.
            mMenuWindow.show();
        } else {
            mMenuWindow.dismiss();
        }
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

    private static int getDimensionPixelSize(Context context, @DimenRes int dimenId) {
        return context.getResources().getDimensionPixelSize(dimenId);
    }

    private PropertyModel createMenuPropertyModel() {
        return new PropertyModel.Builder(ExtensionsMenuProperties.ALL_KEYS)
                .with(ExtensionsMenuProperties.CLOSE_CLICK_LISTENER, (view) -> closeMenu())
                .with(
                        ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER,
                        (view) -> openUrlFromMenu(UrlConstants.CHROME_WEBSTORE_URL))
                .with(
                        ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER,
                        (view) -> openUrlFromMenu(UrlConstants.CHROME_EXTENSIONS_URL))
                .build();
    }

    private static void setUpExtensionsRecyclerView(
            View contentView, Context context, ModelList extensionModels) {
        RecyclerView extensionRecyclerView = contentView.findViewById(R.id.extensions_menu_items);
        SimpleRecyclerViewAdapter extensionsAdapter =
                new SimpleRecyclerViewAdapter(extensionModels);

        extensionsAdapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.extensions_menu_item),
                ExtensionsMenuItemViewBinder::bind);

        extensionRecyclerView.setAdapter(extensionsAdapter);
        extensionRecyclerView.setLayoutManager(new LinearLayoutManager(context));
    }

    private static AnchoredPopupWindow createPopupWindow(
            Context context,
            View decorView,
            ListMenuButton extensionsMenuButton,
            View contentView) {
        ViewRectProvider anchoredViewRectProvider = new ViewRectProvider(extensionsMenuButton);
        int toolbarHeight = extensionsMenuButton.getHeight();
        int iconHeight =
                getDimensionPixelSize(
                        context, org.chromium.chrome.browser.toolbar.R.dimen.toolbar_icon_height);
        int paddingVertical = (toolbarHeight - iconHeight) / 2;
        anchoredViewRectProvider.setInsetPx(0, paddingVertical, 0, paddingVertical);
        anchoredViewRectProvider.setIncludePadding(true);

        AnchoredPopupWindow menuWindow =
                new AnchoredPopupWindow(
                        context,
                        decorView,
                        AppCompatResources.getDrawable(
                                context, R.drawable.extensions_menu_bg_tinted),
                        contentView,
                        anchoredViewRectProvider);

        return menuWindow;
    }

    @Override
    public void destroy() {
        mChangeProcessor.destroy();
        mMediator.destroy();
    }

    @VisibleForTesting
    View getContentView() {
        return mContentView;
    }
}
