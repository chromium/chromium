// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class ExtensionActionHoverCardViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ExtensionActionHoverCardProperties.ACTION_TITLE) {
            TextView titleView = view.findViewById(R.id.extension_action_hover_card_title);
            titleView.setText(model.get(ExtensionActionHoverCardProperties.ACTION_TITLE));
        } else if (propertyKey == ExtensionActionHoverCardProperties.SITE_ACCESS_TITLE
                || propertyKey == ExtensionActionHoverCardProperties.SITE_ACCESS_DESC) {
            String title = model.get(ExtensionActionHoverCardProperties.SITE_ACCESS_TITLE);
            String desc = model.get(ExtensionActionHoverCardProperties.SITE_ACCESS_DESC);

            View divider = view.findViewById(R.id.extension_action_hover_card_site_access_divider);
            LinearLayout container =
                    view.findViewById(R.id.extension_action_hover_card_site_access_container);
            TextView titleView =
                    view.findViewById(R.id.extension_action_hover_card_site_access_title);
            TextView descView =
                    view.findViewById(R.id.extension_action_hover_card_site_access_description);

            if (title != null) {
                divider.setVisibility(View.VISIBLE);
                container.setVisibility(View.VISIBLE);
                titleView.setText(title);

                assert desc != null;
                descView.setText(desc);
                descView.setVisibility(View.VISIBLE);
            } else {
                divider.setVisibility(View.GONE);
                container.setVisibility(View.GONE);
            }

        } else if (propertyKey == ExtensionActionHoverCardProperties.POLICY_TEXT) {
            String policy = model.get(ExtensionActionHoverCardProperties.POLICY_TEXT);

            View divider = view.findViewById(R.id.extension_action_hover_card_policy_divider);
            LinearLayout container =
                    view.findViewById(R.id.extension_action_hover_card_policy_container);
            TextView policyView = view.findViewById(R.id.extension_action_hover_card_policy_text);

            if (policy != null) {
                divider.setVisibility(View.VISIBLE);
                container.setVisibility(View.VISIBLE);
                policyView.setText(policy);
            } else {
                divider.setVisibility(View.GONE);
                container.setVisibility(View.GONE);
            }
        }
    }
}
