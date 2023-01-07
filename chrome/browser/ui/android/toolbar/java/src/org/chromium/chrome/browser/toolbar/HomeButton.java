// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;

/**
 * The home button.
 * TODO(crbug.com/1056422): Fix the visibility bug on NTP.
 */
public class HomeButton extends ListMenuButton implements MenuItem.OnMenuItemClickListener {
    @VisibleForTesting
    public static final int ID_SETTINGS = 0;

    private Callback<Context> mOnMenuClickCallback;
    private Supplier<Boolean> mIsManagedByPolicySupplier;

    // Test related members
    private static boolean sSaveContextMenuForTests;
    private ModelList mMenuForTests;

    public HomeButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        final int homeButtonIcon = R.drawable.btn_toolbar_home;
        setImageDrawable(ContextCompat.getDrawable(context, homeButtonIcon));
    }

    /**
     * Initialize home button.
     * @param homepageVisibility Observable used to react on homepage visibility change.
     * @param onMenuClickCallback Callback for menu click event on homepage.
     * @param isHomepageManagedByPolicy Supplier that tells if homepage is managed by policy.
     */
    public void init(ObservableSupplier<Boolean> homepageVisibility,
            Callback<Context> onMenuClickCallback,
            ObservableSupplier<Boolean> isHomepageManagedByPolicy) {
        Callback<Boolean> contextUpdateCallback = (visible) -> updateContextMenuListener();
        homepageVisibility.addObserver(contextUpdateCallback);
        isHomepageManagedByPolicy.addObserver(contextUpdateCallback);
        mOnMenuClickCallback = onMenuClickCallback;
        mIsManagedByPolicySupplier = isHomepageManagedByPolicy;
        updateContextMenuListener();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        assert !mIsManagedByPolicySupplier.get();
        assert item.getItemId() == ID_SETTINGS;
        assert mOnMenuClickCallback != null;

        mOnMenuClickCallback.onResult(getContext());
        return true;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("HomeButton.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("HomeButton.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    private void updateContextMenuListener() {
        if (!mIsManagedByPolicySupplier.get() && mOnMenuClickCallback != null) {
            setOnLongClickListener(view -> {
                setDelegateForMenu(view);
                ((ListMenuButton) view).showMenu();
                return true;
            });
        } else {
            setLongClickable(false);
        }
    }

    private void setDelegateForMenu(View anchorView) {
        RectProvider rectProvider = MenuBuilderHelper.getRectProvider(anchorView);
        ModelList menuItems = buildMenuItems();
        mMenuForTests = menuItems;
        BasicListMenu listMenu = new BasicListMenu(
                getContext(), menuItems, (model) -> mOnMenuClickCallback.onResult(getContext()));
        ListMenuButtonDelegate delegate = new ListMenuButtonDelegate() {
            @Override
            public ListMenu getListMenu() {
                return listMenu;
            }

            @Override
            public RectProvider getRectProvider(View listMenuButton) {
                return rectProvider;
            }
        };
        setDelegate(delegate, false);
    }

    public ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        itemList.add(buildMenuListItem(
                R.string.options_homepage_edit_title, ID_SETTINGS, R.drawable.ic_edit_24dp));
        return itemList;
    }

    /**
     * @param saveContextMenuForTests Whether we want to store the context menu for testing
     */
    @VisibleForTesting
    public static void setSaveContextMenuForTests(boolean saveContextMenuForTests) {
        sSaveContextMenuForTests = saveContextMenuForTests;
    }

    /**
     * @return Latest context menu created.
     */
    @VisibleForTesting
    public ModelList getMenuForTests() {
        return mMenuForTests;
    }
}
