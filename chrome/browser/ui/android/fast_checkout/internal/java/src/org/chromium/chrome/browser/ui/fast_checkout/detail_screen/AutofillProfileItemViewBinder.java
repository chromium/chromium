// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties.AUTOFILL_PROFILE;
import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties.ON_CLICK_LISTENER;

import android.view.ViewGroup;

import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * AutofillProfileItemViewBinder contains factory and bind methods for
 * {@link AutofillProfileItemViewHolder}.
 */
public class AutofillProfileItemViewBinder {
    // TODO(crbug.com/1355310): Make proper factory function that can also deal with
    // credit card items and footer items.
    static AutofillProfileItemViewHolder createViewHolder(ViewGroup parent, int itemType) {
        // Only one type of item is supported.
        assert itemType == DetailItemType.PROFILE;
        return new AutofillProfileItemViewHolder(
                parent, R.layout.fast_checkout_autofill_profile_item);
    }

    /** Connects the model contained in item with viewHolder. */
    static void connectPropertyModel(
            AutofillProfileItemViewHolder viewHolder, MVCListAdapter.ListItem item) {
        PropertyModelChangeProcessor.create(
                item.model, viewHolder, AutofillProfileItemViewBinder::bind);
    }

    /** Binds the item view with to the model properties. */
    static void bind(
            PropertyModel model, AutofillProfileItemViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == AUTOFILL_PROFILE) {
            FastCheckoutAutofillProfile profile = model.get(AUTOFILL_PROFILE);
            // TODO(crbug.com/1355310): Update the view.
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.itemView.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        }
    }
}
