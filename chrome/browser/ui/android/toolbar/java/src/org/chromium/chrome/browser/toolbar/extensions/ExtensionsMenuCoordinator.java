// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.divider.MaterialDivider;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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

/**
 * Coordinator for the extensions menu, accessed from the puzzle icon in the toolbar. This class is
 * responsible for the button and the menu.
 */
@NullMarked
public class ExtensionsMenuCoordinator implements Destroyable {
    private final Context mContext;
    private final ListMenuButton mExtensionsMenuButton;
    private final MaterialDivider mExtensionsMenuTabSwitcherDivider;
    private final ThemeColorProvider mThemeColorProvider;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private final TabCreator mTabCreator;
    private final View mContentView;
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final ModelList mExtensionModels;

    private final ThemeColorProvider.TintObserver mTintObserver = this::onTintChanged;
    private final Callback<Profile> mProfileUpdatedCallback = this::onProfileUpdated;

    @Nullable private AnchoredPopupWindow mMenuWindow;
    @Nullable private Profile mProfile;
    @Nullable @VisibleForTesting ExtensionsMenuMediator mMediator;

    private boolean mShouldShowMenu;
    private boolean mExtensionDataReady;

    /**
     * Constructor.
     *
     * @param context The context for this component.
     * @param extensionsMenuButton The puzzle icon in the toolbar.
     * @param extensionsMenuTabSwitcherDivider The divider between the extensions menu and the tab
     *     switcher.
     * @param themeColorProvider The provider for theme colors.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     */
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            MaterialDivider extensionsMenuTabSwitcherDivider,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator) {
        this(
                context,
                extensionsMenuButton,
                extensionsMenuTabSwitcherDivider,
                themeColorProvider,
                profileSupplier,
                currentTabSupplier,
                tabCreator,
                null);
    }

    @VisibleForTesting
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            MaterialDivider extensionsMenuTabSwitcherDivider,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            @Nullable AnchoredPopupWindow menuWindow) {
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mTabCreator = tabCreator;

        mExtensionsMenuButton = extensionsMenuButton;
        mExtensionsMenuButton.setOnClickListener(this::onClick);

        mExtensionsMenuTabSwitcherDivider = extensionsMenuTabSwitcherDivider;

        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(mTintObserver);

        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mProfileUpdatedCallback);

        mContentView = LayoutInflater.from(mContext).inflate(R.layout.extensions_menu, null, false);

        PropertyModel model = createMenuPropertyModel();

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mContentView, ExtensionsMenuViewBinder::bind);

        mExtensionModels = new ModelList();
        setUpExtensionsRecyclerView(mContentView, mContext, mExtensionModels);

        mMenuWindow = menuWindow;

        mMediator =
                new ExtensionsMenuMediator(
                        mProfileSupplier,
                        mCurrentTabSupplier,
                        mExtensionModels,
                        () -> {
                            mExtensionDataReady = true;
                            updateWindowVisibility();
                        });
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        mProfile = profile;

        // TODO(crbug.com/422307625): Remove this check once extensions are ready for dogfooding.
        int visibility = View.GONE;
        if (mProfile != null) {
            ExtensionActionsBridge extensionActionsBridge = ExtensionActionsBridge.get(mProfile);
            if (extensionActionsBridge != null && extensionActionsBridge.extensionsEnabled()) {
                visibility = View.VISIBLE;
            }
        }

        mExtensionsMenuButton.setVisibility(visibility);
        mExtensionsMenuTabSwitcherDivider.setVisibility(visibility);
    }

    void onClick(View view) {
        showMenu();
    }

    /** Show the extensions menu (potentially async). */
    @VisibleForTesting
    void showMenu() {
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
            if (mMenuWindow == null) {
                View decorView = ((Activity) mContext).getWindow().getDecorView();
                mMenuWindow =
                        createPopupWindow(mContext, decorView, mExtensionsMenuButton, mContentView);
            }

            // We have to make sure that the extension data created in the mediator is ready
            // before we can show the menu window.
            mMenuWindow.show();
        } else {
            if (mMenuWindow != null) mMenuWindow.dismiss();
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

    public void onTintChanged(
            @Nullable ColorStateList tintList,
            @Nullable ColorStateList activityFocusTintList,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mExtensionsMenuButton, activityFocusTintList);
    }

    public void updateButtonBackground(int backgroundResource) {
        mExtensionsMenuButton.setBackgroundResource(backgroundResource);
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

        return new AnchoredPopupWindow(
                context,
                decorView,
                AppCompatResources.getDrawable(context, R.drawable.extensions_menu_bg_tinted),
                contentView,
                anchoredViewRectProvider);
    }

    @Override
    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        mExtensionsMenuButton.setOnClickListener(null);
        mThemeColorProvider.removeTintObserver(mTintObserver);
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
        mProfile = null;
        mChangeProcessor.destroy();
    }

    @VisibleForTesting
    View getContentView() {
        return mContentView;
    }
}
