// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.Clipboard;
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
public class ToolbarLongPressMenuHandler {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MenuItemType.MOVE_ADDRESS_BAR_TO, MenuItemType.COPY_LINK})
    public @interface MenuItemType {
        int MOVE_ADDRESS_BAR_TO = 0;
        int COPY_LINK = 1;
    }

    private PopupWindow mPopupMenu;
    private final int mAppMenuShadowLength;
    private final int mEdgeToTextDistance;
    private final int mUrlBarMargin;
    @NonNull private final Context mContext;
    @NonNull private final ObservableSupplier<Profile> mProfileSupplier;
    @NonNull private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    @NonNull private final Supplier<String> mUrlBarTextSupplier;
    @NonNull private final Supplier<ViewRectProvider> mUrlBarViewRectProviderSupplier;
    @Nullable private final OnLongClickListener mOnLongClickListener;
    @NonNull private final SharedPreferencesManager mSharedPreferencesManager;

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
            Supplier<String> urlBarTextSupplier,
            Supplier<ViewRectProvider> urlBarViewRectProviderSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mUrlBarTextSupplier = urlBarTextSupplier;
        mUrlBarViewRectProviderSupplier = urlBarViewRectProviderSupplier;

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
                        + context.getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);
        mUrlBarMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.url_bar_vertical_margin);
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
        boolean onTop =
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        view.getContext(),
                        buildMenuItems(onTop),
                        (model) -> {
                            handleMenuClick(model.get(ListMenuItemProperties.MENU_ITEM_ID));
                            mPopupMenu.dismiss();
                        });

        View menuListView = listMenu.getContentView();

        mPopupMenu = UiWidgetFactory.getInstance().createPopupWindow(view.getContext());
        mPopupMenu.setFocusable(true);
        mPopupMenu.setOutsideTouchable(true);
        mPopupMenu.setWidth(listMenu.getMaxItemWidth() + mAppMenuShadowLength * 2);
        mPopupMenu.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupMenu.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mPopupMenu.setContentView(menuListView);

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
                        /* iconId= */ 0));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.toolbar_copy_link, MenuItemType.COPY_LINK, /* iconId= */ 0));
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
        boolean onTop =
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, !onTop);
    }

    private void handleCopyLink() {
        Clipboard.getInstance().copyUrlToClipboard(new GURL(mUrlBarTextSupplier.get()));
    }

    @VisibleForTesting
    int[] calculateShowLocation(boolean onTop, boolean isRtl, BasicListMenu listMenu) {
        ViewRectProvider viewRectProvider = mUrlBarViewRectProviderSupplier.get();
        viewRectProvider.setIncludePadding(true);
        viewRectProvider.setMarginPx(0, mUrlBarMargin, 0, mUrlBarMargin);
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
            y = urlBarRect.bottom;
        } else {
            // The long press menu will appear above the toolbar.

            y = urlBarRect.top - menuHeight;
        }
        return new int[] {x, y};
    }

    public PopupWindow getPopupWindowForTesting() {
        return mPopupMenu;
    }
}
