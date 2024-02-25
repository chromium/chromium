// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.FooterItemProperties.LABEL;
import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.FooterItemProperties.ON_CLICK_HANDLER;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A simple binder class for footer items. */
class FooterItemViewBinder {
    /** Creates a view for footer items on the detail sheet. */
    static View create(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.fast_checkout_footer_item, parent, false);
    }

    /** Binds the item view for the footer with to the model properties. */
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LABEL) {
            TextViewWithCompoundDrawables newItemTextView =
                    view.findViewById(R.id.fast_checkout_add_new_item_label);
            newItemTextView.setText(model.get(LABEL));
            newItemTextView.setContentDescription(
                    view.getContext().getResources().getString(model.get(LABEL))
                            + ", "
                            + view.getContext()
                                    .getResources()
                                    .getString(
                                            R.string
                                                    .fast_checkout_detail_screen_non_selected_description));
        } else if (propertyKey == ON_CLICK_HANDLER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_HANDLER).run());
        }
    }
}
