// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Coordinator for the bottom bar introductory promo dialog. */
@NullMarked
public class BottomBarPromoDialogCoordinator {
    // TODO(crbug.com/517591009): Remove when implementing logic.
    @SuppressWarnings("unused")
    private final View mDialogView;

    public BottomBarPromoDialogCoordinator(Context context) {
        mDialogView =
                LayoutInflater.from(context).inflate(R.layout.bottom_bar_promo_dialog_view, null);
    }
}
