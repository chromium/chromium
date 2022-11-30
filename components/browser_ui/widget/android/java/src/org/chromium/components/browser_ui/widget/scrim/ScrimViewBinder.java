// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The Class responsible for binding properties to and updating the scrim. */
class ScrimViewBinder {
    static void bind(PropertyModel model, ScrimView view, PropertyKey propertyKey) {
        if (ScrimProperties.TOP_MARGIN == propertyKey) {
            // Noop; this is not used until the anchor is set as the view won't have layout params
            // until it is attached to its parent.

        } else if (ScrimProperties.AFFECTS_STATUS_BAR == propertyKey) {
            // Noop; the mediator handles this interaction.

        } else if (ScrimProperties.ANCHOR_VIEW == propertyKey) {
            View anchor = model.get(ScrimProperties.ANCHOR_VIEW);

            UiUtils.removeViewFromParent(view);

            // The only case where a null anchor is valid is when the scrim is being reset after
            // use.
            if (anchor == null) return;

            view.placeScrimInHierarchy(
                    anchor, model.get(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW));

            assert view.getLayoutParams() instanceof MarginLayoutParams;
            ((MarginLayoutParams) view.getLayoutParams()).topMargin =
                    model.get(ScrimProperties.TOP_MARGIN);

        } else if (ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW == propertyKey) {
            // Noop; this is not used until the anchor is set.

        } else if (ScrimProperties.CLICK_DELEGATE == propertyKey) {
            // Attach the click listener only if one is set otherwise this will interfere with the
            // GESTURE_DETECTOR property.
            // TODO(mdjones): Consider including click handling as part of the more general gesture
            //                GESTURE_DETECTOR.
            if (model.get(ScrimProperties.CLICK_DELEGATE) != null) {
                view.setOnClickListener((v) -> model.get(ScrimProperties.CLICK_DELEGATE).run());
            }

        } else if (ScrimProperties.VISIBILITY_CALLBACK == propertyKey) {
            // Noop; callback methods are handled in the mediator.

        } else if (ScrimProperties.ALPHA == propertyKey) {
            float alpha = model.get(ScrimProperties.ALPHA);

            view.setAlpha(alpha);

            int targetVisibility = alpha <= 0 ? View.GONE : View.VISIBLE;
            if (view.getVisibility() != targetVisibility) view.setVisibility(targetVisibility);

        } else if (ScrimProperties.BACKGROUND_COLOR == propertyKey) {
            // If background drawable is set, we don't use the background color.
            if (model.get(ScrimProperties.BACKGROUND_DRAWABLE) != null) return;

            view.setBackgroundColor(model.get(ScrimProperties.BACKGROUND_COLOR));

        } else if (ScrimProperties.BACKGROUND_DRAWABLE == propertyKey) {
            if (model.get(ScrimProperties.BACKGROUND_DRAWABLE) == null) return;
            view.setBackgroundDrawable(model.get(ScrimProperties.BACKGROUND_DRAWABLE));

        } else if (ScrimProperties.GESTURE_DETECTOR == propertyKey) {
            // Noop; gesture handling is delegated out to the mediator.
        } else if (ScrimProperties.AFFECTS_NAVIGATION_BAR == propertyKey) {
            // Noop; the mediator handles this interaction.
        }
    }
}
