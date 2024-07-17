// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.app.Activity;
import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.ViewUtils;

import java.util.List;

/** Utility methods and classes for testing modal dialogs. */
public class ModalDialogTestUtils {
    /** Test observer that notifies dialog dismissal. */
    public interface TestDialogDismissedObserver {
        /**
         * Called when dialog is dismissed.
         * @param dismissalCause The dismissal cause.
         */
        void onDialogDismissed(@DialogDismissalCause int dismissalCause);
    }

    /** @return A {@link PropertyModel} of a modal dialog that is used for testing. */
    public static PropertyModel createDialog(
            Activity activity,
            ModalDialogManager manager,
            String title,
            @Nullable TestDialogDismissedObserver observer) {
        return createDialog(
                activity,
                manager,
                title,
                observer,
                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE);
    }

    /**
     * @return A {@link PropertyModel} of a modal dialog that is used for testing with
     *         primary or negative button filled.
     */
    public static PropertyModel createDialog(
            Activity activity,
            ModalDialogManager manager,
            String title,
            @Nullable TestDialogDismissedObserver observer,
            @ModalDialogProperties.ButtonStyles int buttonStyles) {
        return createDialog(
                activity,
                manager,
                title,
                observer,
                buttonStyles,
                ModalDialogProperties.DialogStyles.NORMAL);
    }

    /**
     * @return A {@link PropertyModel} of a modal dialog that is used for testing with
     *         dialog style
     */
    public static PropertyModel createDialogWithDialogStyle(
            Activity activity,
            ModalDialogManager manager,
            String title,
            @Nullable TestDialogDismissedObserver observer,
            @ModalDialogProperties.DialogStyles int dialogStyles) {
        return createDialog(
                activity,
                manager,
                title,
                observer,
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                dialogStyles);
    }

    /**
     * @return A {@link PropertyModel} of a modal dialog that is used for testing with primary or
     *     negative button filled and dialog style.
     */
    public static PropertyModel createDialog(
            Activity activity,
            ModalDialogManager manager,
            String title,
            @Nullable TestDialogDismissedObserver observer,
            @ModalDialogProperties.ButtonStyles int buttonStyles,
            @ModalDialogProperties.DialogStyles int dialogStyles) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModalDialogProperties.Controller controller =
                            new ModalDialogProperties.Controller() {
                                @Override
                                public void onDismiss(
                                        PropertyModel model,
                                        @DialogDismissalCause int dismissalCause) {
                                    if (observer != null) {
                                        observer.onDialogDismissed(dismissalCause);
                                    }
                                }

                                @Override
                                public void onClick(PropertyModel model, int buttonType) {
                                    switch (buttonType) {
                                        case ModalDialogProperties.ButtonType.POSITIVE:
                                        case ModalDialogProperties.ButtonType.NEGATIVE:
                                            manager.dismissDialog(
                                                    model, DialogDismissalCause.UNKNOWN);
                                            break;
                                        default:
                                            Assert.fail("Unknown button type: " + buttonType);
                                    }
                                }
                            };
                    Resources resources = activity.getResources();
                    return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                            .with(ModalDialogProperties.CONTROLLER, controller)
                            .with(ModalDialogProperties.TITLE, title)
                            .with(
                                    ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                    resources,
                                    R.string.ok)
                            .with(
                                    ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                    resources,
                                    R.string.cancel)
                            .with(ModalDialogProperties.BUTTON_STYLES, buttonStyles)
                            .with(ModalDialogProperties.DIALOG_STYLES, dialogStyles)
                            .build();
                });
    }

    /**
     * Shows a dialog on the specified {@link ModalDialogManager} on the UI thread and waits for it
     * to be visible.
     * @param manager The {@link ModalDialogManager} used to show the dialog.
     * @param model The {@link PropertyModel} for the dialog to show.
     * @param dialogType The {@link ModalDialogType} of the dialog to show.
     */
    public static void showDialog(
            ModalDialogManager manager, PropertyModel model, @ModalDialogType int dialogType) {
        showDialog(manager, model, dialogType, true);
    }

    /**
     * Shows a dialog on the specified {@link ModalDialogManager} on the UI thread.
     *
     * @param manager The {@link ModalDialogManager} used to show the dialog.
     * @param model The {@link PropertyModel} for the dialog to show.
     * @param dialogType The {@link ModalDialogType} of the dialog to show.
     * @param waitForShow Whether to wait for the dialog to be shown. Use false if the enqueued
     *     dialog is not expected to show immediately.
     */
    public static void showDialog(
            ModalDialogManager manager,
            PropertyModel model,
            @ModalDialogType int dialogType,
            boolean waitForShow) {
        ThreadUtils.runOnUiThreadBlocking(() -> manager.showDialog(model, dialogType));
        if (waitForShow) {
            ViewUtils.waitForVisibleView(withId(R.id.modal_dialog_view));
        }
    }

    /**
     * Shows a dialog that's in the root view in the specified {@link ModalDialogManager} on the UI
     * thread.
     *
     * @param manager The {@link ModalDialogManager} used to show the dialog.
     * @param model The {@link PropertyModel} for the dialog to show.
     * @param dialogType The {@link ModalDialogType} of the dialog to show.
     */
    public static void showDialogInRoot(
            ModalDialogManager manager, PropertyModel model, @ModalDialogType int dialogType) {
        ThreadUtils.runOnUiThreadBlocking(() -> manager.showDialog(model, dialogType));
        ViewUtils.waitForDialogViewCheckingState(
                withId(R.id.modal_dialog_view), ViewUtils.VIEW_VISIBLE);
    }

    /** Checks whether the number of pending dialogs of a specified type is as expected. */
    public static void checkPendingSize(
            ModalDialogManager manager, @ModalDialogType int dialogType, int expected) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List list = manager.getPendingDialogsForTest(dialogType);
                    Assert.assertEquals(expected, list != null ? list.size() : 0);
                });
    }

    /**
     * Checks whether the current presenter of the {@link ModalDialogManager} is as expected. If
     * {@code dialogType} is null, then the expected current presenter should be null.
     */
    public static void checkCurrentPresenter(
            ModalDialogManager manager, @Nullable Integer dialogType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (dialogType == null) {
                        Assert.assertFalse(manager.isShowing());
                        Assert.assertNull(manager.getCurrentPresenterForTest());
                    } else {
                        Assert.assertTrue(manager.isShowing());
                        Assert.assertEquals(
                                manager.getPresenterForTest(dialogType),
                                manager.getCurrentPresenterForTest());
                    }
                });
    }

    /**
     * Checks whether the dialog dismissal cause is as expected. If {@code expectedDismissalCause}
     * is null, then the check is skipped.
     */
    public static void checkDialogDismissalCause(
            @Nullable Integer expectedDismissalCause, @DialogDismissalCause int dismissalCause) {
        if (expectedDismissalCause == null) return;
        Assert.assertEquals(expectedDismissalCause.intValue(), dismissalCause);
    }

    /**
     * @param modelBuilder The builder for the modal dialog view model.
     * @param view The {@link ModalDialogView} that should be bound.
     * @return The {@link PropertyModel} that binds the {@code view}.
     */
    public static PropertyModel createModel(
            PropertyModel.Builder modelBuilder, ModalDialogView view) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model = modelBuilder.build();
                    PropertyModelChangeProcessor.create(model, view, new ModalDialogViewBinder());
                    return model;
                });
    }
}
