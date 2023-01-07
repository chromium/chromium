// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the detail screens (Autofill profile selection, credit card selection)
 * of the Fast Checkout bottom sheet.
 */
public class DetailScreenCoordinator {
    /**
     * Sets up the view of the detail screen, puts it into a {@link
     * DetailScreenViewBinder.ViewHolder} and connects it to the PropertyModel by setting up a model
     * change processor.
     */
    public DetailScreenCoordinator(Context context, View view, PropertyModel model) {
        Toolbar toolbar = (Toolbar) view.findViewById(R.id.action_bar);
        assert toolbar != null;
        toolbar.inflateMenu(R.menu.fast_checkout_toolbar_menu);
        Drawable tintedBackIcon = TintedDrawable.constructTintedDrawable(toolbar.getContext(),
                R.drawable.ic_arrow_back_white_24dp, R.color.default_icon_color_tint_list);
        toolbar.setNavigationIcon(tintedBackIcon);
        toolbar.setNavigationContentDescription(
                R.string.fast_checkout_back_to_home_screen_icon_description);

        RecyclerView recyclerView =
                view.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        recyclerView.addItemDecoration(
                new DetailItemDecoration(context.getResources().getDimensionPixelSize(
                        R.dimen.fast_checkout_detail_sheet_spacing_vertical)));

        DetailScreenViewBinder.ViewHolder viewHolder =
                new DetailScreenViewBinder.ViewHolder(context, view);

        PropertyModelChangeProcessor.create(model, viewHolder, DetailScreenViewBinder::bind);

        // TODO(crbug.com/1355310): Make sure that scrolling works as expected.
    }
}
