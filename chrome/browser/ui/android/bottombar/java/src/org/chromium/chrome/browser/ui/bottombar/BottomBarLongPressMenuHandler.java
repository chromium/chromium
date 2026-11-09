// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.annotation.Px;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.LongPressMenuEvent;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;
import org.chromium.ui.widget.ViewRectProvider;

/** Handler for the popup menu shown when long-pressing the bottom bar. */
@NullMarked
class BottomBarLongPressMenuHandler implements Destroyable {
    private final Context mContext;
    private final BottomBarView mBottomBarView;
    private final @Px int mMenuOverlapPx;
    private @Nullable AnchoredPopupWindow mPopupWindow;

    /**
     * Creates a new {@link BottomBarLongPressMenuHandler}.
     *
     * @param context The activity {@link Context} used to style the popup and open Settings.
     * @param bottomBarView The {@link BottomBarView} that anchors the popup menu.
     */
    BottomBarLongPressMenuHandler(Context context, BottomBarView bottomBarView) {
        mContext = context;
        mBottomBarView = bottomBarView;
        mMenuOverlapPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_long_press_menu_overlap);
    }

    /**
     * Shows the long-press menu centered horizontally just above the bottom bar.
     *
     * @return True if the popup menu was shown, false otherwise.
     */
    public boolean showMenu() {
        dismiss();
        // The bottom controls can go INVISIBLE while the long-press is still pending, and the
        // popup would then float over an empty bar.
        if (!mBottomBarView.isShown() || !mBottomBarView.isLaidOut()) return false;

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        mContext,
                        buildMenuItems(),
                        (_, _) -> {
                            dismiss();
                            BottomBarMetrics.recordLongPressMenuEvent(
                                    LongPressMenuEvent.CUSTOMIZE_APPEARANCE_CLICKED);
                            SettingsNavigationFactory.createSettingsNavigation()
                                    .startSettings(mContext, SettingsFragment.APPEARANCE);
                        });

        // Content width includes the padding around the items, so add it here.
        View contentView = listMenu.getContentView();
        int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        int desiredContentWidth = listMenu.getMaxItemWidth() + lateralPadding;

        // Moving the anchor's top edge down makes the menu overlap the top of the bar.
        ViewRectProvider anchorRectProvider = new ViewRectProvider(mBottomBarView);
        anchorRectProvider.setIncludePadding(true);
        anchorRectProvider.setInsetPx(0, mMenuOverlapPx, 0, 0);

        AnchoredPopupWindow popupWindow =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                mBottomBarView,
                                new ColorDrawable(Color.TRANSPARENT),
                                _ -> contentView,
                                anchorRectProvider)
                        .setPreferredHorizontalOrientation(HorizontalOrientation.CENTER)
                        .setPreferredVerticalOrientation(VerticalOrientation.ABOVE)
                        .setDesiredContentWidth(desiredContentWidth)
                        .setAnimationStyle(R.style.PopupWindowAnimRaiseup)
                        .setFocusable(true)
                        .setOutsideTouchable(true)
                        .setDismissOnScreenSizeChange(true)
                        .addOnDismissListener(this::dismiss)
                        .build();
        mPopupWindow = popupWindow;
        popupWindow.show();

        // show() skips popups that can't get a touchable size, e.g. in a tiny multi-window
        // pane, but still leaves the anchor rect provider observing the bar. Clean up and
        // report that nothing was shown, so the long-press gets no haptic feedback.
        if (!popupWindow.isShowing()) {
            popupWindow.dismiss();
            anchorRectProvider.stopObserving();
            mPopupWindow = null;
            return false;
        }
        BottomBarMetrics.recordLongPressMenuEvent(LongPressMenuEvent.SHOWN);
        return true;
    }

    /** Dismisses the long-press popup menu if it is currently showing. */
    public void dismiss() {
        if (mPopupWindow != null) {
            AnchoredPopupWindow popupWindow = mPopupWindow;
            mPopupWindow = null;
            popupWindow.dismiss();
        }
    }

    /** Dismisses the popup menu when the bottom bar goes away. */
    @Override
    public void destroy() {
        dismiss();
    }

    /** Returns whether the popup menu is currently showing. */
    public boolean isShowing() {
        return mPopupWindow != null && mPopupWindow.isShowing();
    }

    private ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        itemList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.bottom_bar_long_press_customize_appearance)
                        .build());
        return itemList;
    }

    @Nullable AnchoredPopupWindow getPopupWindowForTesting() {
        return mPopupWindow;
    }
}
