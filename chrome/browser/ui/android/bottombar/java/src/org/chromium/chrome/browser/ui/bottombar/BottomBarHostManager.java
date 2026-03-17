// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Manages the host of the bottom bar. */
@NullMarked
public class BottomBarHostManager {
    /** The host of the bottom bar. */
    @Target(ElementType.TYPE_USE)
    @IntDef({Host.TABBED, Host.HUB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Host {
        /** The current host is the regular tabbed mode bottom app bar. */
        int TABBED = 0;

        /** The current host is the Hub bottom toolbar. */
        int HUB = 1;
    }

    private @Nullable BottomBar mBottomBar;

    /**
     * Registers the bottom bar. This method should only ever be called once.
     *
     * @param bottomBar The bottom bar to register.
     */
    public void registerBottomBar(BottomBar bottomBar) {
        assert mBottomBar == null : "registerBottomBar should only be called once";
        mBottomBar = bottomBar;
    }

    /**
     * Takes ownership of the bottom bar view for the given host. Detaches the bottom bar's view
     * from its current parent and attaches it to the new host.
     *
     * @param host The new host taking ownership.
     * @param attachCallback A callback to attach the bottom bar view to the new host.
     */
    public void takeOwnership(@Host int host, Callback<View> attachCallback) {
        assert mBottomBar != null : "Bottom bar must be registered before taking ownership";

        View view = mBottomBar.getView();
        if (view.getParent() instanceof ViewGroup viewGroup) {
            viewGroup.removeView(view);
        }

        attachCallback.onResult(view);
        mBottomBar.setParent(host);
    }
}
