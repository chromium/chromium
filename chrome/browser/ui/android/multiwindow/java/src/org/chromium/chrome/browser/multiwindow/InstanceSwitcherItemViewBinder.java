// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class InstanceSwitcherItemViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (InstanceSwitcherItemProperties.FAVICON == propertyKey) {
            ((ImageView) view.findViewById(R.id.favicon))
                    .setImageDrawable(model.get(InstanceSwitcherItemProperties.FAVICON));

        } else if (InstanceSwitcherItemProperties.TITLE == propertyKey) {
            TextView titleView = (TextView) view.findViewById(R.id.title);
            String text = model.get(InstanceSwitcherItemProperties.TITLE);
            if (text != null) {
                titleView.setText(text);
            } else {
                titleView.setVisibility(View.GONE);
            }

        } else if (InstanceSwitcherItemProperties.DESC == propertyKey) {
            TextView descView = (TextView) view.findViewById(R.id.desc);
            String text = model.get(InstanceSwitcherItemProperties.DESC);
            if (text != null) {
                descView.setText(text);
            } else {
                descView.setVisibility(View.GONE);
            }

        } else if (InstanceSwitcherItemProperties.CURRENT == propertyKey) {
            boolean current = model.get(InstanceSwitcherItemProperties.CURRENT);
            view.findViewById(R.id.current).setVisibility(current ? View.VISIBLE : View.INVISIBLE);
            // Do not show 3-dot submenu for the current instance.
            view.findViewById(R.id.more).setVisibility(current ? View.INVISIBLE : View.VISIBLE);

        } else if (InstanceSwitcherItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(InstanceSwitcherItemProperties.CLICK_LISTENER));

        } else if (InstanceSwitcherItemProperties.MORE_MENU == propertyKey) {
            ListMenuButtonDelegate delegate = model.get(InstanceSwitcherItemProperties.MORE_MENU);
            ((ListMenuButton) view.findViewById(R.id.more)).setDelegate(delegate);

        } else if (InstanceSwitcherItemProperties.ENABLE_COMMAND == propertyKey) {
            View newWindow = view.findViewById(R.id.new_window);
            View maxInfo = view.findViewById(R.id.max_info);
            boolean enabled = model.get(InstanceSwitcherItemProperties.ENABLE_COMMAND);
            newWindow.setVisibility(enabled ? View.VISIBLE : View.GONE);
            maxInfo.setVisibility(enabled ? View.GONE : View.VISIBLE);
        }
    }
}
