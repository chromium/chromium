// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

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
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogResult;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ActionConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionConfirmationDialogUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ConfirmationDialogResult mConfirmationDialogResult;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelArgumentCaptor;

    private Context mContext;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_AppCompat);
        mContext = activity;
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
                mConfirmationDialogResult);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        assertEquals("Title", propertyModel.get(ModalDialogProperties.TITLE));
        assertEquals("Confirm", propertyModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals(descriptionTextView.getText(), "Learn more");
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
                mConfirmationDialogResult);

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
                mConfirmationDialogResult);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(propertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mConfirmationDialogResult)
                .onDismiss(ButtonClickResult.POSITIVE, /* stopShowing= */ false);
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
                mConfirmationDialogResult);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mConfirmationDialogResult)
                .onDismiss(ButtonClickResult.NEGATIVE, /* stopShowing= */ false);
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
                mConfirmationDialogResult);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(propertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mConfirmationDialogResult)
                .onDismiss(ButtonClickResult.POSITIVE, /* stopShowing= */ true);
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
                mConfirmationDialogResult);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelArgumentCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();

        View customView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);

        Controller controller = propertyModel.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mConfirmationDialogResult)
                .onDismiss(ButtonClickResult.NEGATIVE, /* stopShowing= */ true);
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
                mConfirmationDialogResult);

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
                mConfirmationDialogResult);

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
        verify(mConfirmationDialogResult)
                .onDismiss(ButtonClickResult.NO_CLICK, /* stopShowing= */ false);
    }
}
