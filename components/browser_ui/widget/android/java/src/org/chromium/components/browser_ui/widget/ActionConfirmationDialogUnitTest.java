// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Looper;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.util.Function;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DismissHandler;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ActionConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionConfirmationDialogUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ConfirmationDialogHandler mConfirmationDialogHandler;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelArgumentCaptor;

    private Context mContext;
    private @Nullable Runnable mDimissLaterRunnable;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;
        configureDismissType(DialogDismissType.DISMISS_IMMEDIATELY);
    }

    private void configureDismissType(@DialogDismissType int dialogDismissType) {
        if (dialogDismissType == DialogDismissType.DISMISS_IMMEDIATELY) {
            when(mConfirmationDialogHandler.onDialogInteracted(
                            any(DismissHandler.class), anyInt(), anyBoolean()))
                    .thenReturn(DialogDismissType.DISMISS_IMMEDIATELY);
        } else {
            doAnswer(
                            (Answer<Integer>)
                                    invocation -> {
                                        DismissHandler dismissHandler =
                                                (DismissHandler) invocation.getArguments()[0];
                                        mDimissLaterRunnable =
                                                dismissHandler.dismissBlocking(
                                                        (int) invocation.getArguments()[1]);
                                        return dialogDismissType;
                                    })
                    .when(mConfirmationDialogHandler)
                    .onDialogInteracted(any(DismissHandler.class), anyInt(), anyBoolean());
        }
    }

    private Function<Resources, String> noSyncResolver(@StringRes int stringRes) {
        return (resources) -> resources.getString(stringRes);
    }

    private Function<Resources, String> syncResolver(@StringRes int stringRes, String account) {
        return (resources) -> resources.getString(stringRes, account);
    }

    @Test
    public void testShowNoSync() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        assertEquals("Title", propertyModel.get(ModalDialogProperties.TITLE));
        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals("Learn more", descriptionTextView.getText());
    }

    @Test
    public void testShowWithSync() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        // chip_remove_icon_content_description can be any string with formal args.
        dialog.show(
                noSyncResolver(R.string.title),
                syncResolver(R.string.chip_remove_icon_content_description, "test@gmail.com"),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals("Remove 'test@gmail.com'", descriptionTextView.getText());
    }

    @Test
    public void testPositiveDismiss() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.POSITIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.POSITIVE),
                        /* stopShowing= */ eq(false));
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testNegativeDismiss() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.NEGATIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.NEGATIVE),
                        /* stopShowing= */ eq(false));
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    public void testPositiveStopShowing() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.POSITIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.POSITIVE),
                        /* stopShowing= */ eq(true));
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testNegativeStopShowing() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.NEGATIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.NEGATIVE),
                        /* stopShowing= */ eq(true));
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    public void testNoStopShowing() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ false,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        assertEquals(View.GONE, stopShowingCheckBox.getVisibility());
    }

    @Test
    public void testDefaultDismiss_CustomNegativeAction() {
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);

        // Stop showing is ignored as default dismiss handler is used. However, positive is the
        // safer default so use that.
        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(propertyModel, DialogDismissalCause.TOUCH_OUTSIDE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.NO_CLICK),
                        /* stopShowing= */ eq(false));
        // The dismiss is already handled by the modal dialog manager. Further calls to dismiss it
        // will not do anything.
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());
    }

    @Test
    public void testAsyncDismiss_Positive() {
        configureDismissType(DialogDismissType.DISMISS_LATER);

        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();
        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.POSITIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.POSITIVE),
                        /* stopShowing= */ eq(false));
        assertTrue(propertyModel.get(ModalDialogProperties.BLOCK_INPUTS));
        assertFalse(propertyModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        // Signal the condition is met first.
        mDimissLaterRunnable.run();
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());

        // Button spinner min-time second.
        shadowOf(Looper.getMainLooper()).runOneTask();
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        controller.onDismiss(propertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Flush the timeout task and verify it isn't run.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyNoMoreInteractions(mModalDialogManager);
    }

    @Test
    public void testAsyncDismiss_Negative() {
        configureDismissType(DialogDismissType.DISMISS_LATER);

        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();
        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.NEGATIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.NEGATIVE),
                        /* stopShowing= */ eq(false));
        assertTrue(propertyModel.get(ModalDialogProperties.BLOCK_INPUTS));
        assertFalse(propertyModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        // Button spinner min-time first.
        shadowOf(Looper.getMainLooper()).runOneTask();
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());

        // Signal the condition is met second.
        mDimissLaterRunnable.run();
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        controller.onDismiss(propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        // Flush the timeout task and verify it isn't run.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyNoMoreInteractions(mModalDialogManager);
    }

    @Test
    public void testAsyncDismiss_Timeout() {
        configureDismissType(DialogDismissType.DISMISS_LATER);

        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                noSyncResolver(R.string.title),
                noSyncResolver(R.string.learn_more),
                R.string.confirm,
                R.string.cancel,
                /* supportStopShowing= */ true,
                mConfirmationDialogHandler);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();
        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModel, ButtonType.NEGATIVE);
        verify(mConfirmationDialogHandler)
                .onDialogInteracted(
                        any(DismissHandler.class),
                        eq(ButtonClickResult.NEGATIVE),
                        /* stopShowing= */ eq(false));
        assertTrue(propertyModel.get(ModalDialogProperties.BLOCK_INPUTS));
        assertFalse(propertyModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        // Flush the min-button duration.
        shadowOf(Looper.getMainLooper()).runOneTask();
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());

        // Flush the timeout task.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mModalDialogManager)
                .dismissDialog(propertyModel, DialogDismissalCause.CLIENT_TIMEOUT);

        // A late update to the runnable should no-op.
        mDimissLaterRunnable.run();
        verifyNoMoreInteractions(mModalDialogManager);
    }
}
