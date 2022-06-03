// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The home button.
 * TODO(crbug.com/1056422): Fix the visibility bug on NTP.
 */
public class HomeButton extends ChromeImageButton
        implements OnCreateContextMenuListener, MenuItem.OnMenuItemClickListener {
    @VisibleForTesting
    public static final int ID_SETTINGS = 0;

    private Callback<Context> mOnMenuClickCallback;
    private Supplier<Boolean> mIsManagedByPolicySupplier;

    // Test related members
    private static boolean sSaveContextMenuForTests;
    private ContextMenu mMenuForTests;

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
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        menu.add(Menu.NONE, ID_SETTINGS, Menu.NONE, R.string.options_homepage_edit_title)
                .setOnMenuItemClickListener(this);

        if (sSaveContextMenuForTests) mMenuForTests = menu;
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
            setOnCreateContextMenuListener(this);
        } else {
            setOnCreateContextMenuListener(null);
            setLongClickable(false);
        }
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
    public ContextMenu getMenuForTests() {
        return mMenuForTests;
    }
}
