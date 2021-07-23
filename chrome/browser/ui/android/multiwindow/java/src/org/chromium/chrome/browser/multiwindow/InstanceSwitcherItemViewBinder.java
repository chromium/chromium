// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class InstanceSwitcherItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (InstanceSwitcherItemProperties.FAVICON == propertyKey) {
            ((ImageView) view.findViewById(R.id.favicon))
                    .setImageDrawable(model.get(InstanceSwitcherItemProperties.FAVICON));

        } else if (InstanceSwitcherItemProperties.TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.title))
                    .setText(model.get(InstanceSwitcherItemProperties.TITLE));

        } else if (InstanceSwitcherItemProperties.DESC == propertyKey) {
            ((TextView) view.findViewById(R.id.desc))
                    .setText(model.get(InstanceSwitcherItemProperties.DESC));
        } else if (InstanceSwitcherItemProperties.CURRENT == propertyKey) {
            int visible = model.get(InstanceSwitcherItemProperties.CURRENT) ? View.VISIBLE
                                                                            : View.INVISIBLE;
            view.findViewById(R.id.current).setVisibility(visible);

        } else if (InstanceSwitcherItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(InstanceSwitcherItemProperties.CLICK_LISTENER));
        } else if (InstanceSwitcherItemProperties.MORE_MENU == propertyKey) {
            ListMenuButtonDelegate delegate = model.get(InstanceSwitcherItemProperties.MORE_MENU);
            ((ListMenuButton) view.findViewById(R.id.more)).setDelegate(delegate);
        }
    }
}
