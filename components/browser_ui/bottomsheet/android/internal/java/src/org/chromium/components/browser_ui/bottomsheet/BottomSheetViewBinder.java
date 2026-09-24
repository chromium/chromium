// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder mapping {@link BottomSheetProperties} to {@link BottomSheetView}. */
@NullMarked
public class BottomSheetViewBinder {
    /**
     * Binds model properties to the presentation view.
     *
     * @param model The model containing bottom sheet properties.
     * @param view The presentation view.
     * @param propertyKey The key of the property to bind.
     */
    public static void bind(PropertyModel model, BottomSheetView view, PropertyKey propertyKey) {
        if (BottomSheetProperties.SHEET_LAYOUT_MODE == propertyKey) {
            view.setSheetLayoutMode(model.get(BottomSheetProperties.SHEET_LAYOUT_MODE));
        } else if (BottomSheetProperties.GLOW_SPEC == propertyKey) {
            view.setGlowSpec(model.get(BottomSheetProperties.GLOW_SPEC));
        } else if (BottomSheetProperties.BACKGROUND_COLOR == propertyKey) {
            view.setSheetBackgroundColor(model.get(BottomSheetProperties.BACKGROUND_COLOR));
        } else if (BottomSheetProperties.CLOSE_BUTTON_VISIBILITY == propertyKey) {
            view.setCloseButtonVisible(model.get(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY));
        } else if (BottomSheetProperties.CLOSE_BUTTON_CLICK_LISTENER == propertyKey) {
            view.setCloseButtonClickListener(
                    model.get(BottomSheetProperties.CLOSE_BUTTON_CLICK_LISTENER));
        } else if (BottomSheetProperties.CONTAINER_TOUCH_ENABLED == propertyKey) {
            view.setContainerTouchEnabled(model.get(BottomSheetProperties.CONTAINER_TOUCH_ENABLED));
        } else if (BottomSheetProperties.CONTENT_VIEW == propertyKey) {
            view.setContentView(model.get(BottomSheetProperties.CONTENT_VIEW));
        } else if (BottomSheetProperties.TOOLBAR_VIEW == propertyKey) {
            view.setToolbarView(model.get(BottomSheetProperties.TOOLBAR_VIEW));
        } else if (BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT == propertyKey) {
            view.setKeyboardCurtainHeight(model.get(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT));
        } else if (BottomSheetProperties.CONTAINER_HEIGHT == propertyKey) {
            view.setContainerHeight(model.get(BottomSheetProperties.CONTAINER_HEIGHT));
        } else if (BottomSheetProperties.SHEET_WIDTH_PX == propertyKey) {
            view.setSheetWidth(model.get(BottomSheetProperties.SHEET_WIDTH_PX));
        } else if (BottomSheetProperties.ACCESSIBILITY_PANE_TITLE == propertyKey) {
            view.setSheetAccessibilityPaneTitle(
                    model.get(BottomSheetProperties.ACCESSIBILITY_PANE_TITLE));
        } else if (BottomSheetProperties.SHEET_TRANSLATION_Y == propertyKey) {
            view.setSheetTranslationY(model.get(BottomSheetProperties.SHEET_TRANSLATION_Y));
        } else if (BottomSheetProperties.SHEET_TRANSLATION_X == propertyKey) {
            view.setSheetTranslationX(model.get(BottomSheetProperties.SHEET_TRANSLATION_X));
        } else if (BottomSheetProperties.HANDLEBAR_VISIBLE == propertyKey) {
            view.setHandlebarVisible(model.get(BottomSheetProperties.HANDLEBAR_VISIBLE));
        } else if (BottomSheetProperties.CONTENT_TOP_MARGIN == propertyKey) {
            view.setContentTopMargin(model.get(BottomSheetProperties.CONTENT_TOP_MARGIN));
        } else if (BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT == propertyKey) {
            view.setVisibleBackgroundHeight(
                    model.get(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT));
        } else if (BottomSheetProperties.SHEET_FOCUSABLE == propertyKey) {
            view.setSheetFocusable(model.get(BottomSheetProperties.SHEET_FOCUSABLE));
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }

    private BottomSheetViewBinder() {}
}
