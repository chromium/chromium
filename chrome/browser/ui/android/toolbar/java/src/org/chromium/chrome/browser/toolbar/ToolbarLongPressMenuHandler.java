// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.settings.AddressBarPreference;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.UiWidgetFactory;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/** The handler for the toolbar long press menu. */
@NullMarked
public class ToolbarLongPressMenuHandler implements ConfigurationChangedObserver {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MenuItemType.MOVE_ADDRESS_BAR_TO, MenuItemType.COPY_LINK})
    public @interface MenuItemType {
        int MOVE_ADDRESS_BAR_TO = 0;
        int COPY_LINK = 1;
    }

    private @Nullable PopupWindow mPopupMenu;
    private final int mAppMenuShadowLength;
    private final int mAdditonalHorizontalPadding;
    private final int mEdgeToTextDistance;
    private final int mUrlBarMargin;
    private final int mMenuOmniboxOverlap;
    private int mScreenWidthDp;
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final Supplier<String> mUrlBarTextSupplier;
    private final Supplier<ViewRectProvider> mUrlBarViewRectProviderSupplier;
    private final @Nullable OnLongClickListener mOnLongClickListener;
    private final SharedPreferencesManager mSharedPreferencesManager;
    private final WindowAndroid mWindowAndroid;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    /**
     * Creates a new {@link ToolbarLongPressMenuHandler}.
     *
     * @param context current context
     */
    public ToolbarLongPressMenuHandler(
            Context context,
            ObservableSupplier<Profile> profileSupplier,
            boolean isCustomTab,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WindowAndroid windowAndroid,
            Supplier<String> urlBarTextSupplier,
            Supplier<ViewRectProvider> urlBarViewRectProviderSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mUrlBarTextSupplier = urlBarTextSupplier;
        mUrlBarViewRectProviderSupplier = urlBarViewRectProviderSupplier;
        mWindowAndroid = windowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);

        mScreenWidthDp = context.getResources().getConfiguration().screenWidthDp;

        if (ToolbarPositionController.isToolbarPositionCustomizationEnabled(context, isCustomTab)) {
            mOnLongClickListener =
                    (view) -> {
                        if (mOmniboxFocusStateSupplier.get()) {
                            // Do nothing if the URL bar has focus during a long press.
                            return false;
                        }

                        displayMenu(view);
                        return true;
                    };
        } else {
            mOnLongClickListener = null;
        }

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mAppMenuShadowLength =
                context.getResources().getDimensionPixelSize(R.dimen.app_menu_shadow_length);
        mAdditonalHorizontalPadding =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_longpress_menu_addtional_horizontal_padding);

        // Long press menu layout
        // +----------------------------------+
        // +            MARGIN                |
        // +     +---+--------------+---+     |
        // |  M  | P |--------------| P |  M  |
        // |  A  | A |--------------| A |  A  |
        // |  R  | D |--------------| D |  R  |
        // |  G  | D |--menu items--| D |  G  |
        // |  I  | I |--------------| I |  I  |
        // |  N  | N |--------------| N |  N  |
        // |     | G |--------------| G |     |
        // +     +---+--------------+---+     |
        // +            MARGIN                |
        // +----------------------------------+
        // ^         ^
        // mEdgeToTextDistance
        mEdgeToTextDistance =
                mAppMenuShadowLength
                        + mAdditonalHorizontalPadding
                        + context.getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);
        mUrlBarMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.url_bar_vertical_margin);
        mMenuOmniboxOverlap =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_longpress_menu_overlap);
    }

    /**
     * Return a long-click listener which shows the toolbar popup menu. Return null if toolbar is in
     * CCT or widgets.
     *
     * @return A long-click listener showing the menu.
     */
    protected @Nullable OnLongClickListener getOnLongClickListener() {
        return mOnLongClickListener;
    }

    private void displayMenu(View view) {
        boolean onTop = AddressBarPreference.isToolbarConfiguredToShowOnTop();

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        view.getContext(),
                        buildMenuItems(onTop),
                        (model) -> {
                            handleMenuClick(model.get(ListMenuItemProperties.MENU_ITEM_ID));
                            assumeNonNull(mPopupMenu);
                            mPopupMenu.dismiss();
                        });

        ListView listView = listMenu.getListView();
        listView.setPaddingRelative(
                listView.getPaddingStart() + mAdditonalHorizontalPadding,
                listView.getPaddingTop(),
                listView.getPaddingEnd() + mAdditonalHorizontalPadding,
                listView.getPaddingBottom());

        mPopupMenu = UiWidgetFactory.getInstance().createPopupWindow(view.getContext());
        mPopupMenu.setFocusable(true);
        mPopupMenu.setOutsideTouchable(true);

        int menuWidthPx =
                listMenu.getMaxItemWidth()
                        + mAdditonalHorizontalPadding * 2
                        + mAppMenuShadowLength * 2;
        int screenWidthPx = DisplayUtil.dpToPx(mWindowAndroid.getDisplay(), mScreenWidthDp);
        mPopupMenu.setWidth(Math.min(menuWidthPx, screenWidthPx));
        mPopupMenu.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupMenu.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mPopupMenu.setContentView(listMenu.getContentView());
        mPopupMenu.setAnimationStyle(
                onTop ? R.style.PopupWindowAnimDropdown : R.style.PopupWindowAnimRaiseup);

        boolean isRtl =
                mContext.getResources().getConfiguration().getLayoutDirection()
                        == View.LAYOUT_DIRECTION_RTL;
        int[] location = calculateShowLocation(onTop, isRtl, listMenu);
        mPopupMenu.showAtLocation(view, Gravity.NO_GRAVITY, location[0], location[1]);

        // Notify the IPH that the User has interacted with the Bottom Toolbar menu.
        // This effectively disables the IPH bubble.
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.BOTTOM_TOOLBAR_MENU_TRIGGERED);
    }

    @VisibleForTesting
    ModelList buildMenuItems(boolean onTop) {
        ModelList itemList = new ModelList();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        onTop
                                ? R.string.toolbar_move_to_the_bottom
                                : R.string.toolbar_move_to_the_top,
                        MenuItemType.MOVE_ADDRESS_BAR_TO,
                        /* startIconId= */ 0));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.toolbar_copy_link, MenuItemType.COPY_LINK, /* startIconId= */ 0));
        return itemList;
    }

    @VisibleForTesting
    void handleMenuClick(int id) {
        if (id == MenuItemType.MOVE_ADDRESS_BAR_TO) {
            handleMoveAddressBarTo();
            return;
        } else if (id == MenuItemType.COPY_LINK) {
            handleCopyLink();
            return;
        }
    }

    private void handleMoveAddressBarTo() {
        boolean onTop = AddressBarPreference.isToolbarConfiguredToShowOnTop();
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, !onTop);
    }

    private void handleCopyLink() {
        Clipboard.getInstance().copyUrlToClipboard(new GURL(mUrlBarTextSupplier.get()));
    }

    @VisibleForTesting
    int[] calculateShowLocation(boolean onTop, boolean isRtl, BasicListMenu listMenu) {
        ViewRectProvider viewRectProvider = mUrlBarViewRectProviderSupplier.get();
        viewRectProvider.setInsetPx(0, mUrlBarMargin, 0, mUrlBarMargin);
        Rect urlBarRect = viewRectProvider.getRect();

        int[] menuDimensions = listMenu.getMenuDimensions();
        int menuWidth = menuDimensions[0];
        int menuHeight = menuDimensions[1];
        // The menu text should be vertically aligned with the text in the URL bar.
        int x =
                isRtl
                        ? urlBarRect.right - menuWidth + mEdgeToTextDistance
                        : urlBarRect.left - mEdgeToTextDistance;
        int y;
        if (onTop) {
            // The long press menu will appear below the toolbar.
            y = urlBarRect.bottom - mMenuOmniboxOverlap;
        } else {
            // The long press menu will appear above the toolbar.

            y = urlBarRect.top - menuHeight + mMenuOmniboxOverlap;
        }
        return new int[] {x, y};
    }

    public @Nullable PopupWindow getPopupWindowForTesting() {
        return mPopupMenu;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (!mLifecycleDispatcher.isNativeInitializationFinished()
                || mScreenWidthDp == newConfig.screenWidthDp) {
            return;
        }

        mScreenWidthDp = newConfig.screenWidthDp;
        if (mPopupMenu != null && mPopupMenu.isShowing()) {
            mPopupMenu.dismiss();
        }
    }

    /** Removes all observers. */
    public void destroy() {
        mLifecycleDispatcher.unregister(this);
    }
}
