// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.header;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.task.PostTask;
import org.chromium.components.autofill_assistant.AssistantSettingsUtil;
import org.chromium.components.autofill_assistant.AssistantTagsForTesting;
import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.carousel.AssistantChipAdapter;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * This class is responsible for pushing updates to the Autofill Assistant header view. These
 * updates are pulled from the {@link AssistantHeaderModel} when a notification of an update is
 * received.
 */
class AssistantHeaderViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantHeaderModel,
                AssistantHeaderViewBinder.ViewHolder, PropertyKey> {
    /** The amount of space to put between the top of the sheet and the bottom of the bubble.*/
    private static final int TEXT_BUBBLE_PIXELS_ABOVE_SHEET = 4;

    private final AccessibilityUtil mAccessibilityUtil;
    private final AssistantSettingsUtil mSettingsUtil;

    public AssistantHeaderViewBinder(
            AccessibilityUtil accessibilityUtil, AssistantSettingsUtil settingsUtil) {
        mAccessibilityUtil = accessibilityUtil;
        mSettingsUtil = settingsUtil;
    }

    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder {
        final Context mContext;
        final AnimatedPoodle mPoodle;
        final ViewGroup mHeader;
        final TextView mStatusMessage;
        final AssistantStepProgressBar mStepProgressBar;
        final ImageView mTtsButton;
        final View mProfileIconView;
        final PopupMenu mProfileIconMenu;
        final MenuItem mProfileIconMenuSettingsMessage;
        final MenuItem mProfileIconMenuSendFeedbackMessage;
        final RecyclerView mChipsContainer;
        @Nullable
        TextBubble mTextBubble;

        ViewHolder(Context context, ViewGroup headerView, AnimatedPoodle poodle,
                RecyclerView chipsContainer) {
            mContext = context;
            mPoodle = poodle;
            mHeader = headerView;
            mStatusMessage = headerView.findViewById(R.id.status_message);
            mStepProgressBar =
                    new AssistantStepProgressBar(headerView.findViewById(R.id.step_progress_bar));
            mTtsButton = (ImageView) headerView.findViewById(R.id.tts_button);
            mProfileIconView = headerView.findViewById(R.id.profile_image);
            mProfileIconMenu = new PopupMenu(context, mProfileIconView);
            mProfileIconMenu.inflate(R.menu.profile_icon_menu);
            mProfileIconMenuSettingsMessage = mProfileIconMenu.getMenu().findItem(R.id.settings);
            mProfileIconMenuSendFeedbackMessage =
                    mProfileIconMenu.getMenu().findItem(R.id.send_feedback);
            mProfileIconView.setOnClickListener(unusedView -> mProfileIconMenu.show());
            mChipsContainer = chipsContainer;
        }

        void disableAnimations(boolean disable) {
            mStepProgressBar.disableAnimations(disable);
            // Hiding the animated poodle seems to be the easiest way to disable its animation since
            // {@link LogoView#setAnimationEnabled(boolean)} is private.
            mPoodle.getView().setVisibility(View.INVISIBLE);
            ((DefaultItemAnimator) mChipsContainer.getItemAnimator())
                    .setSupportsChangeAnimations(!disable);
        }

        void updateProgressBarVisibility(boolean visible) {
            mStepProgressBar.setVisible(visible);
        }
    }

    @Override
    public void bind(AssistantHeaderModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantHeaderModel.STATUS_MESSAGE == propertyKey) {
            String message = model.get(AssistantHeaderModel.STATUS_MESSAGE);
            AssistantTextUtils.applyVisualAppearanceTags(view.mStatusMessage, message, null);
            view.mStatusMessage.announceForAccessibility(view.mStatusMessage.getText());
        } else if (AssistantHeaderModel.PROFILE_ICON_MENU_SETTINGS_MESSAGE == propertyKey) {
            view.mProfileIconMenuSettingsMessage.setTitle(
                    model.get(AssistantHeaderModel.PROFILE_ICON_MENU_SETTINGS_MESSAGE));
        } else if (AssistantHeaderModel.PROFILE_ICON_MENU_SEND_FEEDBACK_MESSAGE == propertyKey) {
            view.mProfileIconMenuSendFeedbackMessage.setTitle(
                    model.get(AssistantHeaderModel.PROFILE_ICON_MENU_SEND_FEEDBACK_MESSAGE));
        } else if (AssistantHeaderModel.PROGRESS_ACTIVE_STEP == propertyKey) {
            int activeStep = model.get(AssistantHeaderModel.PROGRESS_ACTIVE_STEP);
            if (activeStep >= 0) {
                view.mStepProgressBar.setActiveStep(activeStep);
            }
        } else if (AssistantHeaderModel.PROGRESS_BAR_ERROR == propertyKey) {
            view.mStepProgressBar.setError(model.get(AssistantHeaderModel.PROGRESS_BAR_ERROR));
        } else if (AssistantHeaderModel.PROGRESS_VISIBLE == propertyKey) {
            view.updateProgressBarVisibility(model.get(AssistantHeaderModel.PROGRESS_VISIBLE));
        } else if (AssistantHeaderModel.STEP_PROGRESS_BAR_ICONS == propertyKey) {
            view.mStepProgressBar.setSteps(model.get(AssistantHeaderModel.STEP_PROGRESS_BAR_ICONS));
            view.mStepProgressBar.disableAnimations(
                    model.get(AssistantHeaderModel.DISABLE_ANIMATIONS_FOR_TESTING));
        } else if (AssistantHeaderModel.SPIN_POODLE == propertyKey) {
            view.mPoodle.setSpinEnabled(model.get(AssistantHeaderModel.SPIN_POODLE));
        } else if (AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK == propertyKey) {
            setProfileMenuListener(view, model.get(AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK));
        } else if (AssistantHeaderModel.CHIPS == propertyKey) {
            view.mChipsContainer.invalidateItemDecorations();
            ((AssistantChipAdapter) view.mChipsContainer.getAdapter())
                    .setChips(model.get(AssistantHeaderModel.CHIPS));
            maybeShowChips(model, view);
        } else if (AssistantHeaderModel.CHIPS_VISIBLE == propertyKey) {
            maybeShowChips(model, view);
        } else if (AssistantHeaderModel.BUBBLE_MESSAGE == propertyKey) {
            showOrDismissBubble(model, view);
        } else if (AssistantHeaderModel.TTS_BUTTON_VISIBLE == propertyKey) {
            showOrHideTtsButton(model, view);
        } else if (AssistantHeaderModel.TTS_BUTTON_STATE == propertyKey) {
            setTtsButtonState(view, model.get(AssistantHeaderModel.TTS_BUTTON_STATE));
        } else if (AssistantHeaderModel.TTS_BUTTON_CALLBACK == propertyKey) {
            setTtsButtonClickListener(view, model.get(AssistantHeaderModel.TTS_BUTTON_CALLBACK));
        } else if (AssistantHeaderModel.DISABLE_ANIMATIONS_FOR_TESTING == propertyKey) {
            view.disableAnimations(model.get(AssistantHeaderModel.DISABLE_ANIMATIONS_FOR_TESTING));
        } else {
            assert false : "Unhandled property detected in AssistantHeaderViewBinder!";
        }
    }

    private void maybeShowChips(AssistantHeaderModel model, ViewHolder view) {
        // The PostTask is necessary as a workaround for the sticky button occasionally not showing,
        // this makes sure that the change happens after any possibly clashing animation currently
        // happening.
        // TODO(b/164389932): Figure out a better fix that doesn't require issuing the change in the
        // following UI iteration.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (model.get(AssistantHeaderModel.CHIPS_VISIBLE)
                    && !model.get(AssistantHeaderModel.CHIPS).isEmpty()) {
                view.mChipsContainer.setVisibility(View.VISIBLE);
                view.mProfileIconView.setVisibility(View.GONE);
            } else {
                view.mChipsContainer.setVisibility(View.GONE);
                view.mProfileIconView.setVisibility(View.VISIBLE);
            }
        });
    }

    private void setProfileMenuListener(ViewHolder view, @Nullable Runnable feedbackCallback) {
        view.mProfileIconMenu.setOnMenuItemClickListener(item -> {
            int itemId = item.getItemId();
            if (itemId == R.id.settings) {
                mSettingsUtil.launch(view.mHeader.getContext());
                return true;
            } else if (itemId == R.id.send_feedback) {
                if (feedbackCallback != null) {
                    feedbackCallback.run();
                }
                return true;
            }

            return false;
        });
    }

    private void showOrDismissBubble(AssistantHeaderModel model, ViewHolder view) {
        String message = model.get(AssistantHeaderModel.BUBBLE_MESSAGE);
        if (message.isEmpty() && view.mTextBubble == null) {
            return;
        }
        if (message.isEmpty() && view.mTextBubble != null) {
            view.mTextBubble.dismiss();
            view.mTextBubble = null;
            return;
        }
        View poodle = view.mPoodle.getView();
        ViewRectProvider anchorRectProvider = new ViewRectProvider(poodle);
        int topOffset = view.mContext.getResources().getDimensionPixelSize(
                                R.dimen.autofill_assistant_root_view_top_padding)
                + TEXT_BUBBLE_PIXELS_ABOVE_SHEET;
        anchorRectProvider.setInsetPx(0, -topOffset, 0, 0);
        view.mTextBubble = new TextBubble(
                /*context = */ view.mContext, /*rootView = */ poodle, /*contentString = */ message,
                /*accessibilityString = */ message, /*showArrow = */ true,
                /*anchorRectProvider = */ anchorRectProvider,
                mAccessibilityUtil.isAccessibilityEnabled());
        view.mTextBubble.setDismissOnTouchInteraction(true);
        view.mTextBubble.show();
    }

    private void showOrHideTtsButton(AssistantHeaderModel model, ViewHolder view) {
        if (model.get(AssistantHeaderModel.TTS_BUTTON_VISIBLE)) {
            view.mTtsButton.setVisibility(View.VISIBLE);
        } else {
            view.mTtsButton.setVisibility(View.GONE);
        }
    }

    private void setTtsButtonClickListener(ViewHolder view, @Nullable Runnable ttsButtonCallback) {
        view.mTtsButton.setOnClickListener(unusedView -> {
            if (ttsButtonCallback != null) {
                ttsButtonCallback.run();
            }
        });
    }

    private void setTtsButtonState(ViewHolder view, @AssistantTtsButtonState int state) {
        switch (state) {
            case AssistantTtsButtonState.DEFAULT:
                view.mTtsButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
                view.mTtsButton.setTag(AssistantTagsForTesting.TTS_ENABLED_ICON_TAG);
                break;
            case AssistantTtsButtonState.PLAYING:
                view.mTtsButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
                view.mTtsButton.setTag(AssistantTagsForTesting.TTS_PLAYING_ICON_TAG);
                break;
            case AssistantTtsButtonState.DISABLED:
                view.mTtsButton.setImageResource(R.drawable.ic_volume_off_white_24dp);
                view.mTtsButton.setTag(AssistantTagsForTesting.TTS_DISABLED_ICON_TAG);
                break;
        }
    }
}
