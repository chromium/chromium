// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.MenuItem;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.Toolbar;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;

/**
 * A toolbar for the selection screens in the FastCheckout bottom sheet. It
 * contains an additional Menu with a settings gear icon.
 */
public class FastCheckoutToolbar extends Toolbar {
    /** The delegate of the class that processes clicks on menu items. */
    public interface Delegate {
        /**
         * The user clicked on the gear to open settings to edit an Autofill profile
         * or a credit card profile.
         */
        public void onOpenSettingsClick();

        /** The user clicked the back icon in the toolbar to return to the home screen. */
        public void onBackIconClick();
    }

    public FastCheckoutToolbar(Context context) {
        this(context, null);
    }

    public FastCheckoutToolbar(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public FastCheckoutToolbar(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        inflateMenu(R.menu.fast_checkout_toolbar_menu);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setBackgroundColor(SemanticColorUtils.getDefaultBgColor(getContext()));
        setTitleTextAppearance(getContext(), R.style.TextAppearance_Headline);
        Drawable tintedBackIcon = TintedDrawable.constructTintedDrawable(getContext(),
                R.drawable.ic_arrow_back_white_24dp, R.color.default_icon_color_tint_list);
        setNavigationIcon(tintedBackIcon);
    }

    /** Binds the items to the Delegate. The delegate must be non-null. */
    public void setDelegate(@NonNull Delegate delegate) {
        assert delegate != null;

        setNavigationOnClickListener((v) -> delegate.onBackIconClick());
        setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                if (item.getItemId() == R.id.settings_menu_id) {
                    delegate.onOpenSettingsClick();
                    return true;
                }
                return false;
            }
        });
    }

    /** Sets the title of the MenuItem for opening settings. */
    public void setSettingsMenuTitle(int resId) {
        MenuItem settingsMenuItem = getMenu().findItem(R.id.settings_menu_id);
        assert settingsMenuItem != null;
        settingsMenuItem.setTitle(resId);
    }
}
