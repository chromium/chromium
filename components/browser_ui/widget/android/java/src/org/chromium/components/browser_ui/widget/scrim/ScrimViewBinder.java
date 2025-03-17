// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.AFFECTS_NAVIGATION_BAR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.AFFECTS_STATUS_BAR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.ALPHA;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.ANCHOR_VIEW;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.BACKGROUND_COLOR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.BOTTOM_MARGIN;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.CLICK_DELEGATE;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.GESTURE_DETECTOR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.TOP_MARGIN;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.TOUCH_EVENT_DELEGATE;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.VISIBILITY_CALLBACK;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The Class responsible for binding properties to and updating the scrim. */
@NullMarked
class ScrimViewBinder {
    static void bind(PropertyModel model, ScrimView view, PropertyKey propertyKey) {
        if (TOP_MARGIN == propertyKey || BOTTOM_MARGIN == propertyKey) {
            // Noop; this is not used until the anchor is set as the view won't have layout params
            // until it is attached to its parent.
        } else if (AFFECTS_STATUS_BAR == propertyKey) {
            // Noop; the mediator handles this interaction.
        } else if (ANCHOR_VIEW == propertyKey) {
            View anchor = model.get(ANCHOR_VIEW);

            UiUtils.removeViewFromParent(view);

            // The only case where a null anchor is valid is when the scrim is being reset after
            // use.
            if (anchor == null) return;

            view.placeScrimInHierarchy(anchor, model.get(SHOW_IN_FRONT_OF_ANCHOR_VIEW));

            assert view.getLayoutParams() instanceof MarginLayoutParams;
            ((MarginLayoutParams) view.getLayoutParams()).topMargin = model.get(TOP_MARGIN);
            ((MarginLayoutParams) view.getLayoutParams()).bottomMargin = model.get(BOTTOM_MARGIN);

        } else if (SHOW_IN_FRONT_OF_ANCHOR_VIEW == propertyKey) {
            // Noop; this is not used until the anchor is set.
        } else if (CLICK_DELEGATE == propertyKey) {
            // Attach the click listener only if one is set otherwise this will interfere with the
            // GESTURE_DETECTOR property.
            // TODO(mdjones): Consider including click handling as part of the more general gesture
            //                GESTURE_DETECTOR.
            if (model.get(CLICK_DELEGATE) != null) {
                view.setOnClickListener((v) -> model.get(CLICK_DELEGATE).run());
            }

        } else if (VISIBILITY_CALLBACK == propertyKey) {
            // Noop; callback methods are handled in the mediator.

        } else if (ALPHA == propertyKey) {
            float alpha = model.get(ALPHA);

            view.setAlpha(alpha);

            int targetVisibility = alpha <= 0 ? View.GONE : View.VISIBLE;
            if (view.getVisibility() != targetVisibility) view.setVisibility(targetVisibility);

        } else if (BACKGROUND_COLOR == propertyKey) {
            view.setBackgroundColor(model.get(BACKGROUND_COLOR));
        } else if (GESTURE_DETECTOR == propertyKey) {
            // Noop; gesture handling is delegated out to the mediator.
        } else if (AFFECTS_NAVIGATION_BAR == propertyKey) {
            // Noop; the mediator handles this interaction.
        } else if (TOUCH_EVENT_DELEGATE == propertyKey) {
            view.setTouchEventDelegate(model.get(TOUCH_EVENT_DELEGATE));
        }
    }
}
