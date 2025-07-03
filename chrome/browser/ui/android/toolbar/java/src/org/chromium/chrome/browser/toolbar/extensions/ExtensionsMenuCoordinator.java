// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Coordinator for the extensions menu, access from the puzzle icon in the toolbar. */
@NullMarked
public class ExtensionsMenuCoordinator implements Destroyable {
    private final Context mContext;
    private final ListMenuButton mExtensionsMenuButton;
    private final AnchoredPopupWindow mMenuWindow;
    private final View mContentView;

    /**
     * Constructor.
     *
     * @param context The context for this component.
     * @param extensionsMenuButton The puzzle icon in the toolbar.
     */
    public ExtensionsMenuCoordinator(Context context, ListMenuButton extensionsMenuButton) {
        this(context, extensionsMenuButton, null);
    }

    @VisibleForTesting
    ExtensionsMenuCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            @Nullable AnchoredPopupWindow menuWindow) {
        mContext = context;
        mExtensionsMenuButton = extensionsMenuButton;

        View decorView = ((Activity) mContext).getWindow().getDecorView();
        ViewRectProvider anchoredViewRectProvider = new ViewRectProvider(mExtensionsMenuButton);
        mContentView = LayoutInflater.from(mContext).inflate(R.layout.extensions_menu, null, false);

        if (menuWindow != null) {
            mMenuWindow = menuWindow;
        } else {
            mMenuWindow =
                    new AnchoredPopupWindow(
                            mContext,
                            decorView,
                            AppCompatResources.getDrawable(
                                    mContext, R.drawable.extensions_menu_bg_tinted),
                            mContentView,
                            anchoredViewRectProvider);
        }
    }

    /** Shows the extensions menu. */
    public void showMenu() {
        mMenuWindow.show();
    }

    @Override
    public void destroy() {}
}
