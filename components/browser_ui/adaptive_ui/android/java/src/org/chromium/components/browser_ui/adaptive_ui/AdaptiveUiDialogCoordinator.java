// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.activity.ComponentDialog;
import androidx.activity.OnBackPressedCallback;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
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
 *
 * <p>Also drives the {@link ModalDialogManager}: {@link #show()} queues the dialog, an in-dialog
 * action or {@link #hide} dismisses it, and the owner hears when it is shown and when it is gone.
 */
@NullMarked
final class AdaptiveUiDialogCoordinator
        implements ModalDialogProperties.Controller, ModalDialogManagerObserver {
    private final PropertyModel mModel;
    private final ModalDialogManager mModalDialogManager;
    private final Runnable mOnShown;
    private final Callback<Integer> mOnDismissed;
    private final PropertyModel mDialogModel;
    private final PropertyModelChangeProcessor<PropertyModel, PropertyModel, PropertyKey>
            mChangeProcessor;
    private final @AdaptiveUiScope int mScope;
    private boolean mDestroyed;
    private @Nullable @DismissReason Integer mRequestedDismissReason;

    /** Whether this is the last dialog the manager created and its onDialogShown is pending. */
    private boolean mWaitingForOnShown;

    private boolean mShown;

    /**
     * @param context The {@link Context} used to inflate the dialog body layout that wraps a custom
     *     view.
     * @param model The feature's model, built with {@link AdaptiveUiProperties#ALL_KEYS}.
     * @param scope The {@link AdaptiveUiScope} of the entry. The model's back press handler is only
     *     forwarded to the dialog for {@link AdaptiveUiScope#WINDOW}.
     * @param modalDialogManager The window's {@link ModalDialogManager} that shows the dialog.
     * @param onShown Runs once, right after the {@link ModalDialogManager} shows the dialog. Not
     *     run again if the dialog is suspended and later shown again.
     * @param onDismissed Receives the {@link DismissReason} exactly once, when the dialog is
     *     dismissed. Prefers any latched reason from {@link #requestDismiss} or {@link #hide},
     *     otherwise translates the {@link DialogDismissalCause} via {@link
     *     #fromDialogDismissalCause}.
     */
    AdaptiveUiDialogCoordinator(
            Context context,
            PropertyModel model,
            @AdaptiveUiScope int scope,
            ModalDialogManager modalDialogManager,
            Runnable onShown,
            Callback<Integer> onDismissed) {
        mModel = model;
        mModalDialogManager = modalDialogManager;
        mOnShown = onShown;
        mOnDismissed = onDismissed;
        mScope = scope;
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

    /** Hands the dialog to the {@link ModalDialogManager}, which may queue it. */
    void show() {
        if (mDestroyed) return;
        mModalDialogManager.addObserver(this);
        mModalDialogManager.showDialog(mDialogModel, getDialogType(mScope));
    }

    /**
     * Dismisses the dialog if it is showing or queued; {@code onDismissed} reports {@code reason}
     * before this returns.
     *
     * @param reason Why the entry is closed.
     */
    void hide(@DismissReason int reason) {
        if (mDestroyed) return;
        setDismissReason(reason);
        // Synchronous for both showing and queued dialogs; onDismiss() reports the reason.
        mModalDialogManager.dismissDialog(mDialogModel, toDialogDismissalCause(reason));
    }

    /** Maps {@link AdaptiveUiScope#WINDOW} to app-modal and transient entries to tab-modal. */
    static @ModalDialogType int getDialogType(@AdaptiveUiScope int scope) {
        return scope == AdaptiveUiScope.WINDOW ? ModalDialogType.APP : ModalDialogType.TAB;
    }

    // ModalDialogManagerObserver implementation.

    @Override
    public void onDialogCreated(PropertyModel model, @Nullable ComponentDialog dialog) {
        mWaitingForOnShown = model == mDialogModel;
    }

    @Override
    public void onDialogShown(View dialogView) {
        if (!mWaitingForOnShown) return;
        mWaitingForOnShown = false;
        // A dialog suspended and shown again is still the same presentation.
        if (mShown) return;
        mShown = true;
        mOnShown.run();
    }

    /**
     * Latches the {@link DismissReason} to report when the dialog is subsequently dismissed. First
     * call wins: ignored if a reason was already latched (by an earlier call or an in-dialog
     * action) or after {@link #destroy()}.
     */
    void setDismissReason(@DismissReason int reason) {
        if (!mDestroyed && mRequestedDismissReason == null) {
            mRequestedDismissReason = reason;
        }
    }

    /**
     * Detaches the {@link PropertyModelChangeProcessor} and releases {@link
     * AdaptiveUiProperties#CUSTOM_VIEW} from any wrapper view so it can be reused in a subsequent
     * presentation. Safe to call more than once.
     */
    void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        mModalDialogManager.removeObserver(this);
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
        @DismissReason
        int reason =
                mRequestedDismissReason != null
                        ? mRequestedDismissReason
                        : fromDialogDismissalCause(dismissalCause);
        destroy();
        mOnDismissed.onResult(reason);
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
        if (isDismissing()) return;
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
        if (isDismissing()) return;
        Runnable callback = mModel.get(callbackKey);
        if (callback != null) callback.run();
        if (mModel.get(keepsOpenKey)) return;
        requestDismiss(reason);
    }

    private void onCloseButtonClicked(View view) {
        requestDismiss(DismissReason.CLOSE_BUTTON);
    }

    /** Whether the dialog is destroyed or a dismissal was already requested. */
    private boolean isDismissing() {
        return mDestroyed || mRequestedDismissReason != null;
    }

    private void requestDismiss(@DismissReason int reason) {
        if (isDismissing()) return;
        mRequestedDismissReason = reason;
        hide(reason);
    }

    // LINT.IfChange(DialogDismissReasonMapping)
    @DialogDismissalCause
    int toDialogDismissalCause(@DismissReason int reason) {
        switch (reason) {
            case DismissReason.BACK_PRESS:
                return mScope == AdaptiveUiScope.WINDOW
                        ? DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                        : DialogDismissalCause.NAVIGATE_BACK;
            case DismissReason.TOUCH_OUTSIDE:
            case DismissReason.SWIPE_DOWN:
                return mScope == AdaptiveUiScope.WINDOW
                        ? DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                        : DialogDismissalCause.TOUCH_OUTSIDE;
            case DismissReason.CLOSE_BUTTON:
            case DismissReason.SECONDARY_ACTION:
                return DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
            case DismissReason.PRIMARY_ACTION:
                return DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
            case DismissReason.NAVIGATION:
                return DialogDismissalCause.NAVIGATE;
            case DismissReason.TAB_SWITCHED:
                return DialogDismissalCause.TAB_SWITCHED;
            case DismissReason.TAB_CLOSED:
                return DialogDismissalCause.TAB_DESTROYED;
            default:
                return DialogDismissalCause.UNKNOWN;
        }
    }

    private static @DismissReason int fromDialogDismissalCause(
            @DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.NAVIGATE_BACK:
                return DismissReason.BACK_PRESS;
            case DialogDismissalCause.TOUCH_OUTSIDE:
            case DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE:
                return DismissReason.TOUCH_OUTSIDE;
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
            case DialogDismissalCause.ACTION_ON_CONTENT:
            case DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED:
                return DismissReason.PRIMARY_ACTION;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
            case DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE:
                return DismissReason.SECONDARY_ACTION;
            case DialogDismissalCause.NAVIGATE:
                return DismissReason.NAVIGATION;
            case DialogDismissalCause.TAB_SWITCHED:
                return DismissReason.TAB_SWITCHED;
            case DialogDismissalCause.TAB_DESTROYED:
            case DialogDismissalCause.ACTIVITY_DESTROYED:
            case DialogDismissalCause.NOT_ATTACHED_TO_WINDOW:
            case DialogDismissalCause.WEB_CONTENTS_DESTROYED:
                return DismissReason.TAB_CLOSED;
            case DialogDismissalCause.UNKNOWN:
            case DialogDismissalCause.DISMISSED_BY_NATIVE:
            case DialogDismissalCause.DIALOG_INTERACTION_DEFERRED:
            case DialogDismissalCause.CLIENT_TIMEOUT:
            default:
                return DismissReason.NAVIGATION;
        }
    }
    // LINT.ThenChange(//components/browser_ui/adaptive_ui/android/java/src/org/chromium/components/browser_ui/adaptive_ui/DismissReason.java:DismissReason)
}
