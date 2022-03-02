// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.header;

import android.support.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.carousel.AssistantChip;
import org.chromium.components.autofill_assistant.generic_ui.AssistantDrawable;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantHeaderModel extends PropertyModel {
    public static final WritableObjectPropertyKey<List<AssistantChip>> CHIPS =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> STATUS_MESSAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> BUBBLE_MESSAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PROFILE_ICON_MENU_SETTINGS_MESSAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PROFILE_ICON_MENU_SEND_FEEDBACK_MESSAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey PROGRESS_ACTIVE_STEP = new WritableIntPropertyKey();

    public static final WritableBooleanPropertyKey PROGRESS_BAR_ERROR =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey PROGRESS_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<List<AssistantDrawable>> STEP_PROGRESS_BAR_ICONS =
            new WritableObjectPropertyKey<>();

    static final WritableBooleanPropertyKey SPIN_POODLE = new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Runnable> FEEDBACK_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey CHIPS_VISIBLE = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey TTS_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableIntPropertyKey TTS_BUTTON_STATE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<Runnable> TTS_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey DISABLE_ANIMATIONS_FOR_TESTING =
            new WritableBooleanPropertyKey();

    public AssistantHeaderModel() {
        super(STATUS_MESSAGE, BUBBLE_MESSAGE, PROFILE_ICON_MENU_SETTINGS_MESSAGE,
                PROFILE_ICON_MENU_SEND_FEEDBACK_MESSAGE, PROGRESS_ACTIVE_STEP, PROGRESS_BAR_ERROR,
                PROGRESS_VISIBLE, STEP_PROGRESS_BAR_ICONS, SPIN_POODLE, FEEDBACK_BUTTON_CALLBACK,
                CHIPS, CHIPS_VISIBLE, TTS_BUTTON_VISIBLE, TTS_BUTTON_STATE, TTS_BUTTON_CALLBACK,
                DISABLE_ANIMATIONS_FOR_TESTING);
        set(CHIPS, new ArrayList<>());
        set(PROGRESS_VISIBLE, true);
        set(TTS_BUTTON_VISIBLE, false);
        set(TTS_BUTTON_STATE, AssistantTtsButtonState.DEFAULT);
    }

    @CalledByNative
    private void setStatusMessage(String statusMessage) {
        set(STATUS_MESSAGE, statusMessage);
    }

    @CalledByNative
    private void setBubbleMessage(String bubbleMessage) {
        set(BUBBLE_MESSAGE, bubbleMessage);
    }

    @CalledByNative
    private void setProfileIconMenuSettingsMessage(String profileIconMenuSettingsMessage) {
        set(PROFILE_ICON_MENU_SETTINGS_MESSAGE, profileIconMenuSettingsMessage);
    }

    @CalledByNative
    private void setProfileIconMenuSendFeedbackMessage(String profileIconMenuSendFeedbackMessage) {
        set(PROFILE_ICON_MENU_SEND_FEEDBACK_MESSAGE, profileIconMenuSendFeedbackMessage);
    }

    @CalledByNative
    private void setProgressActiveStep(int activeStep) {
        set(PROGRESS_ACTIVE_STEP, activeStep);
    }

    @CalledByNative
    private void setProgressBarErrorState(boolean error) {
        set(PROGRESS_BAR_ERROR, error);
    }

    @CalledByNative
    private void setProgressVisible(boolean visible) {
        set(PROGRESS_VISIBLE, visible);
    }

    @CalledByNative
    private static List<AssistantDrawable> createIconList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addStepProgressBarIcon(
            List<AssistantDrawable> icons, AssistantDrawable icon) {
        icons.add(icon);
    }

    @CalledByNative
    private void setStepProgressBarIcons(List<AssistantDrawable> icons) {
        set(STEP_PROGRESS_BAR_ICONS, icons);
        // Reset progress bar entries.
        set(PROGRESS_ACTIVE_STEP, -1);
        set(PROGRESS_BAR_ERROR, false);
    }

    @CalledByNative
    private void setSpinPoodle(boolean enabled) {
        set(SPIN_POODLE, enabled);
    }

    @CalledByNative
    private void setDelegate(AssistantHeaderDelegate delegate) {
        // TODO(b/196945756): Replace callbacks with a single delegate field.
        set(FEEDBACK_BUTTON_CALLBACK, delegate::onFeedbackButtonClicked);
        set(TTS_BUTTON_CALLBACK, delegate::onTtsButtonClicked);
    }

    @CalledByNative
    private void setTtsButtonVisible(boolean visible) {
        set(TTS_BUTTON_VISIBLE, visible);
    }

    @CalledByNative
    private void setTtsButtonState(@AssistantTtsButtonState int state) {
        set(TTS_BUTTON_STATE, state);
    }

    @CalledByNative
    private void setDisableAnimations(boolean disableAnimations) {
        set(DISABLE_ANIMATIONS_FOR_TESTING, disableAnimations);
    }

    @CalledByNative
    @VisibleForTesting
    public void setChips(List<AssistantChip> chips) {
        // Move last chip (cancel) to first position. For legacy reasons, native builds this list
        // such that the cancel chip is last, but the regular carousel will show it in the left-most
        // position and the header should mirror this.
        if (chips.size() > 1) {
            chips.add(0, chips.remove(chips.size() - 1));
        }
        set(CHIPS, chips);
    }
}
