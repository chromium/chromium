// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Coordinator for the Account Menu toolbar popup on desktop Android. */
@NullMarked
public class AccountMenuCoordinator {
    // Threshold to ignore trailing clicks on the signin button right after being dismissed.
    private static final long CLICK_TO_DISMISS_THRESHOLD_MS = 200;

    private final Context mContext;
    private final View mContentView;
    private final AccountMenuMediator mMediator;
    private final SimpleRecyclerViewAdapter mAdapter;

    private @Nullable AnchoredPopupWindow mPopupWindow;
    private long mLastDismissTimeMs;

    public AccountMenuCoordinator(
            Context context,
            Profile profile,
            WindowAndroid windowAndroid,
            @Nullable BottomSheetSigninAndHistorySyncCoordinator signinCoordinator,
            SigninAndHistorySyncActivityLauncher signinLauncher) {
        mContext = context;
        mContentView = LayoutInflater.from(context).inflate(R.layout.account_menu, null);

        RecyclerView recyclerView = (RecyclerView) mContentView;
        recyclerView.setLayoutManager(new LinearLayoutManager(context));
        recyclerView.setItemAnimator(null);

        ModelList modelList = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(modelList);
        mAdapter.registerType(
                ItemType.PROMO_CARD,
                new LayoutViewBuilder<>(R.layout.account_menu_promo_card),
                AccountMenuViewBinder::bindPromoCard);
        mAdapter.registerType(
                ItemType.IDENTITY_CARD,
                new LayoutViewBuilder<>(R.layout.account_menu_identity_card),
                AccountMenuViewBinder::bindIdentityCard);
        mAdapter.registerType(
                ItemType.MENU_ITEM,
                new LayoutViewBuilder<>(R.layout.account_menu_item),
                AccountMenuViewBinder::bindMenuItem);
        mAdapter.registerType(
                ItemType.DIVIDER,
                new LayoutViewBuilder<>(R.layout.account_menu_divider),
                (model, view, key) -> {});
        recyclerView.setAdapter(mAdapter);

        mMediator =
                new AccountMenuMediator(
                        context,
                        profile,
                        windowAndroid,
                        modelList,
                        signinCoordinator,
                        signinLauncher,
                        this::dismiss);
    }

    /** Shows the account menu popup anchored to the provided signin button view. */
    public void show(ListMenuButton anchorView) {
        if (mPopupWindow != null && mPopupWindow.isShowing()) {
            dismiss();
            return;
        }

        if (TimeUtils.elapsedRealtimeMillis() - mLastDismissTimeMs
                < CLICK_TO_DISMISS_THRESHOLD_MS) {
            return;
        }

        mMediator.updateMenuItems();

        mPopupWindow = createPopupWindow(anchorView);
        mPopupWindow.show();

        anchorView.setIsPressed(true);
    }

    /** Dismisses the popup window if it is currently showing. */
    public void dismiss() {
        if (mPopupWindow != null && mPopupWindow.isShowing()) {
            mPopupWindow.dismiss();
        }
    }

    /** Destroys and cleans up the account menu coordinator. */
    public void destroy() {
        dismiss();
        mMediator.destroy();
        mAdapter.destroy();
    }

    private AnchoredPopupWindow createPopupWindow(ListMenuButton anchorView) {
        // Ensure mContentView is detached from any previous popup window parent.
        UiUtils.removeViewFromParent(mContentView);

        int maxMenuWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.account_menu_max_width);
        return new AnchoredPopupWindow.Builder(
                        mContext,
                        anchorView,
                        new ColorDrawable(Color.TRANSPARENT),
                        () -> mContentView,
                        MenuBuilderHelper.getRectProvider(anchorView))
                .addOnDismissListener(() -> onPopupDismissed(anchorView))
                .setMaxWidth(maxMenuWidth)
                .setAnimateFromAnchor(true)
                .setDismissOnScreenSizeChange(true)
                .setFocusable(true)
                .setHorizontalOverlapAnchor(true)
                .setOutsideTouchable(true)
                .build();
    }

    private void onPopupDismissed(ListMenuButton anchorView) {
        anchorView.setIsPressed(false);
        mLastDismissTimeMs = TimeUtils.elapsedRealtimeMillis();
        mPopupWindow = null;
    }
}
