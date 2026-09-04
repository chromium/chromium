// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** StatusViewBinder observes StatusModel changes and triggers StatusView updates. */
@NullMarked
class StatusViewBinder implements ViewBinder<PropertyModel, StatusView, PropertyKey> {
    StatusViewBinder() {}

    @Override
    public void bind(PropertyModel model, StatusView view, PropertyKey propertyKey) {
        if (propertyKey == StatusProperties.ALPHA) {
            view.setAlpha(model.get(StatusProperties.ALPHA));
        } else if (propertyKey == StatusProperties.ANIMATIONS_ENABLED) {
            view.setAnimationsEnabled(model.get(StatusProperties.ANIMATIONS_ENABLED));
        } else if (propertyKey == StatusProperties.INCOGNITO_BADGE_VISIBLE) {
            view.setIncognitoBadgeVisibility(model.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        } else if (propertyKey == StatusProperties.SEPARATOR_COLOR) {
            view.setSeparatorColor(model.get(StatusProperties.SEPARATOR_COLOR));
        } else if (propertyKey == StatusProperties.SHOW_STATUS_VIEW) {
            int visibility =
                    model.get(StatusProperties.SHOW_STATUS_VIEW) ? View.VISIBLE : View.GONE;
            view.setVisibility(visibility);
        } else if (propertyKey
                == StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES) {
            view.setStatusAccessibilityDoubleTapDescription(
                    model.get(StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES));
        } else if (propertyKey == StatusProperties.STATUS_ACCESSIBILITY_TOAST_RES) {
            view.setStatusAccessibilityToast(
                    model.get(StatusProperties.STATUS_ACCESSIBILITY_TOAST_RES));
        } else if (propertyKey == StatusProperties.STATUS_CLICK_LISTENER) {
            view.setStatusClickListener(model.get(StatusProperties.STATUS_CLICK_LISTENER));
        } else if (propertyKey == StatusProperties.STATUS_ICON_CORNER_RADIUS) {
            view.setCornerRadiusRes(model.get(StatusProperties.STATUS_ICON_CORNER_RADIUS));
        } else if (propertyKey == StatusProperties.STATUS_ICON_DESCRIPTION_RES) {
            view.setStatusIconDescription(model.get(StatusProperties.STATUS_ICON_DESCRIPTION_RES));
        } else if (propertyKey == StatusProperties.STATUS_ICON_RESOURCE) {
            StatusIconResource res = model.get(StatusProperties.STATUS_ICON_RESOURCE);
            if (res == null) {
                view.setStatusIconResources(null, StatusView.IconTransitionType.CROSSFADE, null);
                return;
            }
            view.setStatusIconResources(
                    res.getDrawable(view.getContext()),
                    res.getTransitionType(),
                    res.getAnimationFinishedCallback());
        } else if (propertyKey == StatusProperties.STATUS_VIEW_BACKGROUND) {
            applyStatusIconAndTooltipProperties(model, view);
        } else if (propertyKey == StatusProperties.STATUS_VIEW_HOVER_ENABLED) {
            view.setHoverEnabled(model.get(StatusProperties.STATUS_VIEW_HOVER_ENABLED));
        } else if (propertyKey == StatusProperties.STATUS_VIEW_TOOLTIP_TEXT) {
            applyStatusIconAndTooltipProperties(model, view);
        } else if (propertyKey == StatusProperties.TRANSLATION_X) {
            view.setTranslationX(model.get(StatusProperties.TRANSLATION_X));
        } else if (propertyKey == StatusProperties.USE_SMALL_WIDGET) {
            var params = view.getLayoutParams();
            boolean useSmallWidget = model.get(StatusProperties.USE_SMALL_WIDGET);
            params.height =
                    useSmallWidget
                            ? MarginLayoutParams.MATCH_PARENT
                            : view.getResources()
                                    .getDimensionPixelSize(R.dimen.location_bar_height);
            view.setLayoutParams(params);
        } else if (propertyKey == StatusProperties.USE_WIDE_STATUS_ICON) {
            view.setMinimumWidth(
                    view.getResources()
                            .getDimensionPixelSize(
                                    model.get(StatusProperties.USE_WIDE_STATUS_ICON)
                                            ? R.dimen.status_view_width_wide
                                            : R.dimen.status_view_width_narrow));
        } else if (propertyKey == StatusProperties.VERBOSE_STATUS_TEXT_COLOR) {
            view.setVerboseStatusTextColor(model.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        } else if (propertyKey == StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES) {
            view.setVerboseStatusTextContent(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        } else if (propertyKey == StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE) {
            view.setVerboseStatusTextVisible(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
            applyStatusIconAndTooltipProperties(model, view);
        } else if (propertyKey == StatusProperties.VERBOSE_STATUS_TEXT_WIDTH) {
            view.setVerboseStatusTextWidth(model.get(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH));

        } else {
            assert false : "Unhandled property update";
        }
    }

    static void applyStatusIconAndTooltipProperties(PropertyModel model, StatusView statusView) {
        statusView.setTooltipText(model.get(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT));
        statusView.maybeSetBackground(model.get(StatusProperties.STATUS_VIEW_BACKGROUND));
    }
}
