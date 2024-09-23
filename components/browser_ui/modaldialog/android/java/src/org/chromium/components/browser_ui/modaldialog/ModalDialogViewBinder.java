// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.text.TextUtils;
import android.view.View;

import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;

/**
 * This class is responsible for binding view properties from {@link ModalDialogProperties} to a
 * {@link ModalDialogView}.
 */
public class ModalDialogViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                PropertyModel, ModalDialogView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
        if (ModalDialogProperties.TITLE == propertyKey) {
            view.setTitle(model.get(ModalDialogProperties.TITLE));
        } else if (ModalDialogProperties.TITLE_MAX_LINES == propertyKey) {
            view.setTitleMaxLines(model.get(ModalDialogProperties.TITLE_MAX_LINES));
        } else if (ModalDialogProperties.TITLE_ICON == propertyKey) {
            view.setTitleIcon(model.get(ModalDialogProperties.TITLE_ICON));
        } else if (ModalDialogProperties.MESSAGE_PARAGRAPH_1 == propertyKey) {
            view.setMessageParagraph1(model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        } else if (ModalDialogProperties.MESSAGE_PARAGRAPH_2 == propertyKey) {
            view.setMessageParagraph2(model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_2));
        } else if (ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST == propertyKey) {
            assert checkFilterTouchConsistency(model);
            assert checkDefaultButtonsNotCombinedWithButtonGroup(model);
            view.setupButtonGroup(model.get(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST));
        } else if (ModalDialogProperties.CUSTOM_VIEW == propertyKey) {
            view.setCustomView(model.get(ModalDialogProperties.CUSTOM_VIEW));
        } else if (ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW == propertyKey) {
            view.setCustomButtonBar(model.get(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_TEXT == propertyKey) {
            assert checkFilterTouchConsistency(model);
            assert checkDefaultButtonsNotCombinedWithButtonGroup(model);
            view.setButtonText(
                    ModalDialogProperties.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setButtonContentDescription(
                    ModalDialogProperties.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(
                    ModalDialogProperties.ButtonType.POSITIVE,
                    !model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_TEXT == propertyKey) {
            assert checkFilterTouchConsistency(model);
            assert checkFilledButtonConsistency(model);
            assert checkDefaultButtonsNotCombinedWithButtonGroup(model);
            view.setButtonText(
                    ModalDialogProperties.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setButtonContentDescription(
                    ModalDialogProperties.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(
                    ModalDialogProperties.ButtonType.NEGATIVE,
                    !model.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.FOOTER_MESSAGE == propertyKey) {
            view.setFooterMessage(model.get(ModalDialogProperties.FOOTER_MESSAGE));
        } else if (ModalDialogProperties.TITLE_SCROLLABLE == propertyKey) {
            view.setTitleScrollable(model.get(ModalDialogProperties.TITLE_SCROLLABLE));
        } else if (ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE == propertyKey) {
            assert checkCustomViewScrollConsistency(model);
            view.setWrapCustomViewInScrollable(
                    model.get(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE));
        } else if (ModalDialogProperties.CONTROLLER == propertyKey) {
            view.setOnButtonClickedCallback(
                    (buttonType) -> {
                        model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
                    });
        } else if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
            // Intentionally left empty since this is a property for the dialog container.
        } else if (ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY == propertyKey) {
            assert checkFilterTouchConsistency(model);
            view.setFilterTouchForSecurity(
                    model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY));
        } else if (ModalDialogProperties.TOUCH_FILTERED_CALLBACK == propertyKey) {
            view.setOnTouchFilteredCallback(
                    model.get(ModalDialogProperties.TOUCH_FILTERED_CALLBACK));
        } else if (ModalDialogProperties.CONTENT_DESCRIPTION == propertyKey) {
            // Intentionally left empty since this is a property used for the dialog container.
        } else if (ModalDialogProperties.BUTTON_STYLES == propertyKey) {
            assert checkFilledButtonConsistency(model);
            assert checkButtonStyleIsOnlyConfiguredWithDefaultButtons(model);
            // Intentionally left empty since this is only read once before the dialog is inflated.
        } else if (ModalDialogProperties.DIALOG_STYLES == propertyKey) {
            int dialogStyle = model.get(ModalDialogProperties.DIALOG_STYLES);
            boolean ignoreWidthConstraints =
                    dialogStyle == ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG
                            || dialogStyle
                                    == ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG
                            || dialogStyle == ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE;
            boolean ignoreHeightConstraint =
                    dialogStyle == ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG
                            || dialogStyle
                                    == ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG
                            || dialogStyle == ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE;
            view.setIgnoreConstraints(ignoreWidthConstraints, ignoreHeightConstraint);
        } else if (ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS == propertyKey) {
            view.setButtonTapProtectionDurationMs(
                    model.get(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
        } else if (ModalDialogProperties.FOCUS_DIALOG == propertyKey) {
            // Intentionally left empty since this is a property for the dialog container.
        } else if (ModalDialogProperties.HORIZONTAL_MARGIN == propertyKey) {
            view.setHorizontalMargin(model.get(ModalDialogProperties.HORIZONTAL_MARGIN));
        } else if (ModalDialogProperties.VERTICAL_MARGIN == propertyKey) {
            view.setVerticalMargin(model.get(ModalDialogProperties.VERTICAL_MARGIN));
        } else {
            assert false : "Unhandled property detected in ModalDialogViewBinder!";
        }
    }

    /**
     * Checks if FILTER_TOUCH_FOR_SECURITY flag is consistent with the set of enabled buttons. Touch
     * event filtering in ModalDialogView is only applied to standard buttons and buttons in a
     * button group. When buttons are hidden, filtering touch events doesn't have effect.
     *
     * @return false if security sensitive dialog doesn't have any standard buttons or button group
     *     buttons configured with a text.
     */
    private static boolean checkFilterTouchConsistency(PropertyModel model) {
        return !model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY)
                || isAnyDefaultButtonWithTextConfigured(model)
                || isButtongroupWithTextButtonsConfigured(model);
    }

    /**
     * Checks if the BUTTON_STYLES property is consistent with the set of enabled buttons. If the
     * primary button (== positive button for dialog view) is in filled state, it could be disabled
     * while the negative button is enabled. On the contrary, if the negative button is filled, it
     * could also be disabled while the primary button is enabled. This check only applies to
     * standard buttons on a modal dialog.
     *
     * @return false if one button is in filled state while the other button doesn't present. True
     *     otherwise.
     */
    private static boolean checkFilledButtonConsistency(PropertyModel model) {
        int styles = model.get(ModalDialogProperties.BUTTON_STYLES);
        if (styles == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE) {
            return !TextUtils.isEmpty(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        } else if (styles == ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED) {
            return !TextUtils.isEmpty(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        } else if (styles == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE) {
            return TextUtils.isEmpty(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        }

        return true;
    }

    /**
     * Checks that BUTTON_STYLES isn't present together with CUSTOM_BUTTON_BAR_VIEW or a button
     * group component, because neither support default positive and negative buttons as well as
     * their styling.
     */
    private static boolean checkButtonStyleIsOnlyConfiguredWithDefaultButtons(PropertyModel model) {
        int styles = model.get(ModalDialogProperties.BUTTON_STYLES);
        View customButtons = model.get(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW);
        ModalDialogProperties.ModalDialogButtonSpec[] buttonGroup =
                model.get(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST);
        return styles == 0 || (customButtons == null && buttonGroup == null);
    }

    /** Checks that default button configurations aren't mixed with button group configurations. */
    private static boolean checkDefaultButtonsNotCombinedWithButtonGroup(PropertyModel model) {
        boolean defaultButtonsConfigured = isAnyDefaultButtonWithTextConfigured(model);
        boolean buttonGroupConfigured = isButtongroupWithTextButtonsConfigured(model);
        return (defaultButtonsConfigured ^ buttonGroupConfigured)
                || (!defaultButtonsConfigured && !buttonGroupConfigured);
    }

    /**
     * Checks that if a custom view that should be shown in a ScrollView, it is not itself a scroll
     * container.
     */
    private static boolean checkCustomViewScrollConsistency(PropertyModel model) {
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        return customView == null
                || model.get(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE)
                        != customView.isScrollContainer();
    }

    private static boolean isButtongroupWithTextButtonsConfigured(PropertyModel model) {
        return model.get(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST) != null
                && Arrays.stream(model.get(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST))
                        .anyMatch(buttonSpec -> !TextUtils.isEmpty(buttonSpec.getText()));
    }

    private static boolean isAnyDefaultButtonWithTextConfigured(PropertyModel model) {
        return !TextUtils.isEmpty(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT))
                || !TextUtils.isEmpty(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }
}
