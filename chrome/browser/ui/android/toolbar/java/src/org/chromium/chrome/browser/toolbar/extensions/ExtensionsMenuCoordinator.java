// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.divider.MaterialDivider;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.RectProvider;

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
    private final ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;
    private final TabCreator mTabCreator;
    private final View mContentView;
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final ModelList mExtensionModels;

    private final ThemeColorProvider.TintObserver mTintObserver = this::onTintChanged;

    @Nullable @VisibleForTesting ExtensionsMenuMediator mMediator;

    private boolean mShouldShowMenuOnInit;

    /**
     * Constructor.
     *
     * @param context The context for this component.
     * @param extensionsMenuButton The puzzle icon in the toolbar.
     * @param extensionsMenuTabSwitcherDivider The divider between the extensions menu and the tab
     *     switcher.
     * @param themeColorProvider The provider for theme colors.
     * @param taskSupplier Supplies the {@link ChromeAndroidTask}.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     */
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            MaterialDivider extensionsMenuTabSwitcherDivider,
            ThemeColorProvider themeColorProvider,
            OneshotSupplier<ChromeAndroidTask> taskSupplier,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            TabCreator tabCreator) {
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mTabCreator = tabCreator;

        mExtensionsMenuButton = extensionsMenuButton;
        mExtensionsMenuButton.setOnClickListener(view -> mShouldShowMenuOnInit = true);
        mExtensionsMenuButton.setMenuMaxWidth(
                context.getResources().getDimensionPixelSize(R.dimen.extension_menu_max_width));

        mExtensionsMenuTabSwitcherDivider = extensionsMenuTabSwitcherDivider;

        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(mTintObserver);

        mContentView = LayoutInflater.from(mContext).inflate(R.layout.extensions_menu, null, false);

        PropertyModel model = createMenuPropertyModel();

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mContentView, ExtensionsMenuViewBinder::bind);

        mExtensionModels = new ModelList();
        setUpExtensionsRecyclerView(mContentView, mContext, mExtensionModels);

        ListMenu listMenu =
                new ListMenu() {
                    @Override
                    public View getContentView() {
                        return mContentView;
                    }

                    @Override
                    public void addContentViewClickRunnable(Runnable runnable) {}

                    @Override
                    public int getMaxItemWidth() {
                        assert false : "Max width item measurement not supported";
                        return 0;
                    }
                };

        mMediator =
                new ExtensionsMenuMediator(
                        mContext,
                        taskSupplier,
                        profileSupplier,
                        mCurrentTabSupplier,
                        mExtensionModels,
                        () -> {
                            mExtensionsMenuButton.setDelegate(
                                    new ListMenuDelegate() {
                                        @Override
                                        public RectProvider getRectProvider(
                                                View listMenuHostingView) {
                                            return MenuBuilderHelper.getRectProvider(
                                                    mExtensionsMenuButton);
                                        }

                                        @Override
                                        public ListMenu getListMenu() {
                                            return listMenu;
                                        }
                                    });
                            if (mShouldShowMenuOnInit) {
                                if (mExtensionsMenuButton.getHost().isMenuShowing()) {
                                    mExtensionsMenuButton.dismiss();
                                } else {
                                    mExtensionsMenuButton.showMenu();
                                }
                                mShouldShowMenuOnInit = false;
                            }
                        },
                        (extensionsSupported) -> {
                            int visibility = extensionsSupported ? View.VISIBLE : View.GONE;
                            mExtensionsMenuButton.setVisibility(visibility);
                            mExtensionsMenuTabSwitcherDivider.setVisibility(visibility);
                        },
                        mExtensionsMenuButton.getRootView());
    }

    private void openUrlFromMenu(String url) {
        mExtensionsMenuButton.dismiss();

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

    private PropertyModel createMenuPropertyModel() {
        return new PropertyModel.Builder(ExtensionsMenuProperties.ALL_KEYS)
                .with(
                        ExtensionsMenuProperties.CLOSE_CLICK_LISTENER,
                        (view) -> mExtensionsMenuButton.dismiss())
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

    @Override
    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        mExtensionsMenuButton.setOnClickListener(null);
        mThemeColorProvider.removeTintObserver(mTintObserver);
        mChangeProcessor.destroy();
    }

    @VisibleForTesting
    View getContentView() {
        return mContentView;
    }
}
