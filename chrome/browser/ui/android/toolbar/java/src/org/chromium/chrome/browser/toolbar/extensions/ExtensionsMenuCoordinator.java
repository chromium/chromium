// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarCoordinatorImpl.MenuButtonPinningDelegate;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuButtonState;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;

/**
 * Coordinator for the extensions menu, accessed from the puzzle icon in the toolbar. This class is
 * responsible for the button and the menu.
 */
@NullMarked
public class ExtensionsMenuCoordinator
        implements Destroyable,
                ExtensionsToolbarBridge.Observer,
                ExtensionsToolbarBridge.MenuDelegate {
    private final Context mContext;
    private final ListMenu mExtensionsMenu;
    private final ListMenuButton mExtensionsMenuButton;
    private final ThemeColorProvider mThemeColorProvider;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final TabCreator mTabCreator;
    private final View mContentView;
    private final Profile mProfile;
    private final PropertyModel mMainPageModel;
    private final PropertyModel mSitePermissionsPageModel;
    private final PropertyModelChangeProcessor mMainPageChangeProcessor;
    private final PropertyModelChangeProcessor mSitePermissionsPageChangeProcessor;
    private final ModelList mExtensionModels;
    private final ChromeAndroidTask mTask;
    private final WindowAndroid mWindowAndroid;
    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final MenuButtonPinningDelegate mMenuButtonPinningDelegate;
    private final ThemeColorProvider.TintObserver mTintObserver = this::onTintChanged;

    @Nullable @VisibleForTesting ExtensionsMenuMediator mMediator;

    /**
     * Constructor.
     *
     * @param context The context for this component.
     * @param extensionsMenuButton The puzzle icon in the toolbar.
     * @param themeColorProvider The provider for theme colors.
     * @param task Supplies the {@link ChromeAndroidTask}.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param profile The current profile.
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     * @param extensionsToolbarBridge {@link ExtensionsToolbarBridge} to use.
     * @param MenuButtonPinningDelegate The {@link MenuButtonPinningDelegate} to handle pinning the
     *     icon.
     */
    public ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            ThemeColorProvider themeColorProvider,
            ChromeAndroidTask task,
            WindowAndroid windowAndroid,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            MenuButtonPinningDelegate menuButtonPinningDelegate) {
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mProfile = profile;
        mTabCreator = tabCreator;
        mTask = task;
        mWindowAndroid = windowAndroid;
        mExtensionsToolbarBridge = extensionsToolbarBridge;
        mMenuButtonPinningDelegate = menuButtonPinningDelegate;

        mExtensionsToolbarBridge.setMenuDelegate(this);

        mContentView = LayoutInflater.from(mContext).inflate(R.layout.extensions_menu, null, false);

        mExtensionsMenu =
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

        mExtensionsMenuButton = extensionsMenuButton;
        mExtensionsMenuButton.setMenuMaxWidth(
                context.getResources().getDimensionPixelSize(R.dimen.extension_menu_max_width));
        mExtensionsMenuButton.setDelegate(
                new ListMenuDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return mExtensionsMenu;
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuHostingView) {
                        return MenuBuilderHelper.getRectProvider(mExtensionsMenuButton);
                    }
                },
                /* overrideOnClickListener= */ true);

        // Menu mediator is created when menu is triggered.
        mExtensionsMenuButton.setOnClickListener(
                (view) -> {
                    TrackerFactory.getTrackerForProfile(mProfile)
                            .notifyEvent(EventConstants.EXTENSIONS_MENU_BUTTON_CLICKED);
                    createMediator();
                });

        mExtensionsMenuButton.addPopupListener(
                new ListMenuHost.PopupMenuShownListener() {
                    @Override
                    public void onPopupMenuShown() {}

                    @Override
                    public void onPopupMenuDismissed() {
                        mMenuButtonPinningDelegate.requestLayoutWithViewUtils();
                        destroyMediator();
                        mExtensionModels.clear();
                    }
                });

        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(mTintObserver);
        mExtensionsToolbarBridge.addObserver(this);

        // Create the main page property model and bind it to its view.
        mMainPageModel = new PropertyModel.Builder(ExtensionsMenuProperties.ALL_KEYS).build();
        setupMainPageModel();
        mMainPageChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMainPageModel, mContentView, ExtensionsMenuViewBinder::bind);

        // Create the site permissions page property model and bind it to its view.
        mSitePermissionsPageModel =
                new PropertyModel.Builder(SitePermissionsPageProperties.ALL_KEYS).build();
        setupSitePermissionsPageModel();
        View sitePermissionsView =
                mContentView.findViewById(R.id.extensions_menu_site_permissions_page);
        mSitePermissionsPageChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mSitePermissionsPageModel,
                        sitePermissionsView,
                        SitePermissionsPageViewBinder::bind);

        mExtensionModels = new ModelList();
        setUpExtensionsRecyclerView(mContentView, mContext, mExtensionModels);
        updateButtonState();
    }

    /**
     * Creates the extensions menu mediator and the associated JNI bridge, passing a runnable to
     * show the menu once the mediator has initialized the action.
     *
     * <p>This should only be called when the menu is about to be shown.
     */
    private void createMediator() {
        if (mMediator != null) {
            return;
        }

        // Ensure we start on the main page.
        mMainPageModel.set(
                ExtensionsMenuProperties.CURRENT_PAGE, ExtensionsMenuProperties.Page.MAIN);

        // Instantiate the mediator, which will initialize the JNI bridge to the native code.
        mMediator =
                new ExtensionsMenuMediator(
                        mContext,
                        mTask,
                        mProfile,
                        mCurrentTabSupplier,
                        mTabCreator,
                        mExtensionsToolbarBridge,
                        mExtensionModels,
                        mMainPageModel,
                        mSitePermissionsPageModel,
                        /* onDismissMenu= */ mExtensionsMenuButton::dismiss,
                        /* onReady= */ () -> {
                            mExtensionsMenuButton.showMenu();
                        });
    }

    /**
     * Destroys the extensions menu mediator.
     *
     * <p>This should be called when the menu is closed.
     */
    private void destroyMediator() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
    }

    public void onTintChanged(
            @Nullable ColorStateList tintList,
            @Nullable ColorStateList activityFocusTintList,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mExtensionsMenuButton, activityFocusTintList);
    }

    /**
     * Updates the pinning state of the menu button in the main page model.
     *
     * @param pinned Whether the menu button is pinned.
     */
    public void setMenuButtonPinned(boolean pinned) {
        mMainPageModel.set(ExtensionsMenuProperties.MENU_BUTTON_PINNED, pinned);
    }

    /** Returns whether the extensions menu is open. */
    public boolean isExtensionsMenuOpen() {
        return mExtensionsMenuButton.getHost().isMenuShowing();
    }

    private void setupMainPageModel() {
        mMainPageModel.set(
                ExtensionsMenuProperties.CLOSE_CLICK_LISTENER,
                (view) -> mExtensionsMenuButton.dismiss());
        mMainPageModel.set(
                ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER,
                (view) -> {
                    if (mMediator != null) {
                        mMediator.onDiscoverExtensionsClicked();
                    }
                });
        mMainPageModel.set(
                ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER,
                (view) -> {
                    if (mMediator != null) {
                        mMediator.onManageExtensionsClicked();
                    }
                });
        mMainPageModel.set(
                ExtensionsMenuProperties.MENU_BUTTON_PINNING_CLICK_LISTENER,
                (view) -> {
                    boolean willBePinned = !mMenuButtonPinningDelegate.isMenuButtonPinned();
                    mMenuButtonPinningDelegate.setMenuButtonPinned(willBePinned);
                    if (!willBePinned) {
                        showManageExtensionsAppMenuIph();
                    }
                });
        mMainPageModel.set(
                ExtensionsMenuProperties.MENU_BUTTON_PINNED,
                mMenuButtonPinningDelegate.isMenuButtonPinned());
        mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, true);
        mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE, true);
        mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED, true);
        mMainPageModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER,
                (buttonView, isChecked) -> {
                    if (mMediator != null) {
                        mMediator.onSiteSettingsToggleChanged(isChecked);
                    }
                });
        mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, "");
        mMainPageModel.set(
                ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE,
                ExtensionsMenuTypes.OptionalSectionType.NONE);
        mMainPageModel.set(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, new ArrayList<>());
        mMainPageModel.set(
                ExtensionsMenuProperties.ALLOW_EXTENSION_CLICK_LISTENER,
                (extensionId) -> {
                    if (mMediator != null) {
                        mMediator.onAllowExtensionClicked(extensionId);
                    }
                });
        mMainPageModel.set(
                ExtensionsMenuProperties.DISMISS_EXTENSION_CLICK_LISTENER,
                (extensionId) -> {
                    if (mMediator != null) {
                        mMediator.onDismissExtensionClicked(extensionId);
                    }
                });
        mMainPageModel.set(
                ExtensionsMenuProperties.RELOAD_CLICK_LISTENER,
                (view) -> {
                    if (mMediator != null) {
                        mMediator.onReloadPageButtonClicked();
                    }
                });
    }

    private void showManageExtensionsAppMenuIph() {
        if (mProfile.shutdownStarted()) return;

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return;

        View anchorView = activity.findViewById(R.id.menu_button_wrapper);
        if (anchorView == null) return;

        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, mProfile, new Handler(Looper.getMainLooper()));

        userEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                activity.getResources(),
                                FeatureConstants.IPH_EXTENSIONS_MANAGE_APP_MENU_FEATURE,
                                R.string.extensions_menu_manage_app_menu_iph,
                                R.string.extensions_menu_manage_app_menu_iph)
                        .setAnchorView(anchorView)
                        .setPreferredHorizontalOrientation(
                                HorizontalOrientation.MAX_AVAILABLE_SPACE)
                        .setHorizontalOverlapAnchor(true)
                        .setRemoveArrow(true)
                        .setInsetRect(new Rect())
                        .build());
    }

    private void setupSitePermissionsPageModel() {
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.BACK_CLICK_LISTENER,
                (view) -> {
                    if (mMediator != null) {
                        mMediator.onBackButtonClicked();
                    }
                });
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.CLOSE_CLICK_LISTENER,
                (view) -> mExtensionsMenuButton.dismiss());
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.MANAGE_EXTENSION_CLICK_LISTENER,
                (view) -> {
                    if (mMediator != null) {
                        mMediator.onManageThisExtensionClicked();
                    }
                });
    }

    private static void setUpExtensionsRecyclerView(
            View contentView, Context context, ModelList extensionModels) {
        RecyclerView extensionRecyclerView = contentView.findViewById(R.id.extensions_menu_items);
        SimpleRecyclerViewAdapter extensionsAdapter =
                new SimpleRecyclerViewAdapter(extensionModels);

        extensionsAdapter.registerType(
                0,
                new LayoutViewBuilder<>(R.layout.extensions_menu_item),
                ExtensionsMenuItemViewBinder::bind);

        extensionRecyclerView.setAdapter(extensionsAdapter);
        extensionRecyclerView.setLayoutManager(new LinearLayoutManager(context));

        extensionRecyclerView.setItemAnimator(null);
    }

    private void updateButtonState() {
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null || currentTab.getWebContents() == null) return;

        int color = SemanticColorUtils.getDefaultIconColor(mContext);

        float density = mContext.getResources().getDisplayMetrics().density;
        int iconSizeDp =
                Math.round(
                        mContext.getResources().getDimension(R.dimen.extensions_toolbar_icon_size)
                                / density);

        ExtensionsMenuButtonState state =
                mExtensionsToolbarBridge.getMenuButtonState(
                        currentTab.getWebContents(), iconSizeDp, iconSizeDp, density, color);

        if (state.getIcon() != null) {
            mExtensionsMenuButton.setImageBitmap(state.getIcon());
        } else {
            // Fallback just in case.
            int iconResId = R.drawable.chrome_extension;
            mExtensionsMenuButton.setImageResource(iconResId);
        }

        mExtensionsMenuButton.setTooltipText(state.getTooltip());
        mExtensionsMenuButton.setContentDescription(state.getAccessibleText());
    }

    @Override
    public void onToolbarControlStateUpdated() {
        updateButtonState();
    }

    @Override
    public void onActiveWebContentsChanged(WebContents webContents) {
        updateButtonState();
    }

    @Override
    public void onActionsInitialized() {
        updateButtonState();
    }

    @Override
    public void onActionAdded(String actionId) {
        updateButtonState();
    }

    @Override
    public void onActionRemoved(String actionId) {
        updateButtonState();
    }

    @Override
    public void onActionUpdated(String actionId) {
        updateButtonState();
    }

    @Override
    public void closeExtensionsMenuIfOpen() {
        mExtensionsMenuButton.dismiss();
    }

    @Override
    public void destroy() {
        destroyMediator();
        mExtensionsMenuButton.setOnClickListener(null);
        mThemeColorProvider.removeTintObserver(mTintObserver);
        mMainPageChangeProcessor.destroy();
        mSitePermissionsPageChangeProcessor.destroy();
    }

    @VisibleForTesting
    View getContentView() {
        return mContentView;
    }
}
