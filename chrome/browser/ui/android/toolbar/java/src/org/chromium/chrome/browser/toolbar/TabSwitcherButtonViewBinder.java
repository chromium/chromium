// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Android view of the tab switcher. These
 * updates are pulled from the {@link TabSwitcherModel} when a notification of an update is
 * received.
 */
public class TabSwitcherButtonViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                PropertyModel, TabSwitcherButtonView, PropertyKey> {
    /**
     * Build a binder that handles interaction between the model and the views that make up the
     * tab switcher.
     */
    public TabSwitcherButtonViewBinder() {}

    @Override
    public final void bind(
            PropertyModel model, TabSwitcherButtonView view, PropertyKey propertyKey) {
        if (TabSwitcherButtonProperties.NUMBER_OF_TABS == propertyKey) {
            view.updateTabCountVisuals(model.get(TabSwitcherButtonProperties.NUMBER_OF_TABS));
        } else if (TabSwitcherButtonProperties.ON_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(TabSwitcherButtonProperties.ON_CLICK_LISTENER));
        } else if (TabSwitcherButtonProperties.ON_LONG_CLICK_LISTENER == propertyKey) {
            view.setOnLongClickListener(
                    model.get(TabSwitcherButtonProperties.ON_LONG_CLICK_LISTENER));
        } else if (TabSwitcherButtonProperties.TINT == propertyKey) {
            view.setTint(model.get(TabSwitcherButtonProperties.TINT));
        } else if (TabSwitcherButtonProperties.IS_ENABLED == propertyKey) {
            view.setEnabled(model.get(TabSwitcherButtonProperties.IS_ENABLED));
        } else {
            assert false : "Unhandled property detected in TabSwitcherViewBinder!";
        }
    }
}
