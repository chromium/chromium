// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.activity.OnBackPressedCallback;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Bridges a feature's {@link AdaptiveUiProperties} {@link PropertyModel} to a {@link
 * ModalDialogProperties} {@link PropertyModel} for a single dialog presentation.
 *
 * <p>Creation-time keys (button presence and styles, accessibility description, and the {@link
 * AdaptiveUiProperties#DESCRIPTION} / {@link AdaptiveUiProperties#CUSTOM_VIEW} body layout) are
 * resolved once when the dialog model is constructed, satisfying {@code ModalDialogViewBinder}'s
 * button and custom-view consistency assertions without {@code CHANGE_CUSTOM_VIEW_OR_BUTTONS}. Live
 * 1:1 keys are bound via {@link PropertyModelChangeProcessor} and {@link
 * AdaptiveUiDialogViewBinder}.
 */
@NullMarked
final class AdaptiveUiDialogCoordinator implements ModalDialogProperties.Controller {
    private final PropertyModel mModel;
    private final Callback<Integer> mDismissRequestHandler;
    private final Callback<Integer> mOnDialogDismissed;
    private final PropertyModel mDialogModel;
    private final PropertyModelChangeProcessor<PropertyModel, PropertyModel, PropertyKey>
            mChangeProcessor;
    private boolean mDismissRequested;
    private boolean mDestroyed;

    /**
     * @param context The {@link Context} used to inflate the dialog body layout that wraps a custom
     *     view.
     * @param model The feature's model, built with {@link AdaptiveUiProperties#ALL_KEYS}.
     * @param scope The {@link AdaptiveUiScope} of the entry. The model's back press handler is only
     *     forwarded to the dialog for {@link AdaptiveUiScope#WINDOW}.
     * @param dismissRequestHandler Receives the {@link DismissReason} when an interaction inside
     *     the dialog (primary button, secondary button, or close button) should close the entry.
     * @param onDialogDismissed Receives the {@link DialogDismissalCause} once the dialog has been
     *     dismissed for any reason, unless {@link #destroy()} was called first.
     */
    AdaptiveUiDialogCoordinator(
            Context context,
            PropertyModel model,
            @AdaptiveUiScope int scope,
            Callback<Integer> dismissRequestHandler,
            Callback<Integer> onDialogDismissed) {
        mModel = model;
        mDismissRequestHandler = dismissRequestHandler;
        mOnDialogDismissed = onDialogDismissed;
        mDialogModel = createDialogModel(context, model, scope);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mDialogModel, AdaptiveUiDialogViewBinder::bind);
    }

    /**
     * Returns the {@link ModalDialogProperties} model to present through {@code
     * ModalDialogManager}.
     */
    PropertyModel getDialogModel() {
        return mDialogModel;
    }

    /**
     * Detaches the {@link PropertyModelChangeProcessor} and releases {@link
     * AdaptiveUiProperties#CUSTOM_VIEW} from any wrapper view so it can be reused in a subsequent
     * presentation. Safe to call more than once.
     */
    void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        mChangeProcessor.destroy();
        View customView = mModel.get(AdaptiveUiProperties.CUSTOM_VIEW);
        if (customView != null) {
            UiUtils.removeViewFromParent(customView);
        }
    }

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            onActionButtonClicked(
                    AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                    DismissReason.PRIMARY_ACTION);
        } else if (buttonType == ButtonType.NEGATIVE) {
            onActionButtonClicked(
                    AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN,
                    DismissReason.SECONDARY_ACTION);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        if (mDestroyed) return;
        destroy();
        mOnDialogDismissed.onResult(dismissalCause);
    }

    private PropertyModel createDialogModel(
            Context context, PropertyModel model, @AdaptiveUiScope int scope) {
        String primaryText = model.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT);
        String secondaryText = model.get(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT);
        boolean hasPrimary = !TextUtils.isEmpty(primaryText);
        boolean hasSecondary = !TextUtils.isEmpty(secondaryText);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                computeButtonStyles(hasPrimary, hasSecondary))
                        .with(
                                ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER,
                                this::onCloseButtonClicked);

        String contentDescription = model.get(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION);
        if (!TextUtils.isEmpty(contentDescription)) {
            builder.with(ModalDialogProperties.CONTENT_DESCRIPTION, contentDescription);
        }

        if (scope == AdaptiveUiScope.WINDOW) {
            builder.with(
                    ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                    new OnBackPressedCallback(/* enabled= */ true) {
                        @Override
                        public void handleOnBackPressed() {
                            onBackPressed();
                        }
                    });
        }

        configureBody(context, model.get(AdaptiveUiProperties.CUSTOM_VIEW), builder);

        return builder.build();
    }

    static @ButtonStyles int computeButtonStyles(boolean hasPrimary, boolean hasSecondary) {
        if (hasPrimary && hasSecondary) {
            return ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE;
        }
        if (hasPrimary) {
            return ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE;
        }
        return ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE;
    }

    private static void configureBody(
            Context context, @Nullable View customView, PropertyModel.Builder builder) {
        if (customView == null) {
            // DESCRIPTION is applied to MESSAGE_PARAGRAPHS by AdaptiveUiDialogViewBinder, both at
            // the initial bind and on live updates.
            return;
        }

        // modal_dialog_view.xml assigns layout_height="0dp" and layout_weight="1" to both the
        // paragraph scroll area and the custom_view slot, splitting their combined height 50/50
        // if both are populated. Always wrap a non-null customView in adaptive_ui_dialog_body so
        // description and customView scroll together and a description added or cleared after
        // creation can be updated live by AdaptiveUiDialogViewBinder.
        ViewGroup body =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.adaptive_ui_dialog_body, /* root= */ null);
        UiUtils.removeViewFromParent(customView);
        body.addView(customView);
        builder.with(ModalDialogProperties.CUSTOM_VIEW, body)
                .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true);
    }

    private void onBackPressed() {
        if (mDestroyed || mDismissRequested) return;
        OnBackPressedCallback handler = mModel.get(AdaptiveUiProperties.BACK_PRESS_HANDLER);
        if (handler != null && handler.isEnabled()) {
            handler.handleOnBackPressed();
            return;
        }
        requestDismiss(DismissReason.BACK_PRESS);
    }

    private void onActionButtonClicked(
            ReadableObjectPropertyKey<@Nullable Runnable> callbackKey,
            ReadableBooleanPropertyKey keepsOpenKey,
            @DismissReason int reason) {
        if (mDestroyed || mDismissRequested) return;
        Runnable callback = mModel.get(callbackKey);
        if (callback != null) callback.run();
        if (mModel.get(keepsOpenKey)) return;
        requestDismiss(reason);
    }

    private void onCloseButtonClicked(View view) {
        requestDismiss(DismissReason.CLOSE_BUTTON);
    }

    private void requestDismiss(@DismissReason int reason) {
        if (mDestroyed || mDismissRequested) return;
        mDismissRequested = true;
        mDismissRequestHandler.onResult(reason);
    }
}
