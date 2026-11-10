// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.activity.OnBackPressedCallback;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.modaldialog.ModalDialogViewBinder;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link AdaptiveUiDialogCoordinator} and {@link AdaptiveUiDialogViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiDialogCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private PropertyModel mOtherDialogModel;

    private Activity mActivity;
    private int mShownCount;
    private final List<Integer> mDialogDismissals = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        // Behave like ModalDialogManager: dismissing a dialog reports it to its controller.
        doAnswer(
                        invocation -> {
                            PropertyModel dialogModel = invocation.getArgument(0);
                            dialogModel
                                    .get(ModalDialogProperties.CONTROLLER)
                                    .onDismiss(dialogModel, invocation.getArgument(1));
                            return null;
                        })
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
    }

    private AdaptiveUiDialogCoordinator createCoordinator(PropertyModel model) {
        return createCoordinator(model, AdaptiveUiScope.TRANSIENT);
    }

    private AdaptiveUiDialogCoordinator createCoordinator(
            PropertyModel model, @AdaptiveUiScope int scope) {
        return new AdaptiveUiDialogCoordinator(
                mActivity,
                model,
                scope,
                mModalDialogManager,
                () -> mShownCount++,
                mDialogDismissals::add);
    }

    private static PropertyModel.Builder newModelBuilder() {
        return new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS);
    }

    // ---------------------------------------------------------------------------------------
    // Reflective coverage.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testAllKeysAreCoveredByBinderOrCoordinator() {
        PropertyModel model = newModelBuilder().build();
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS).build();
        for (PropertyKey key : AdaptiveUiProperties.ALL_KEYS) {
            if (AdaptiveUiDialogViewBinder.COORDINATOR_HANDLED_KEYS.contains(key)) continue;
            // bind() asserts when it encounters a key that is neither bound nor coordinator
            // handled, so reaching the end without an AssertionError means the key is bound.
            AdaptiveUiDialogViewBinder.bind(model, dialogModel, key);
        }
    }

    @Test
    public void testCoordinatorHandledKeysAreExpectedSet() {
        Set<PropertyKey> expected =
                Set.of(
                        AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                        AdaptiveUiProperties.CUSTOM_VIEW,
                        AdaptiveUiProperties.BACK_PRESS_HANDLER,
                        AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                        AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                        AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                        AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN);
        assertEquals(expected, AdaptiveUiDialogViewBinder.COORDINATOR_HANDLED_KEYS);
        assertTrue(
                "Primary/secondary button text must be bound live by the binder",
                !AdaptiveUiDialogViewBinder.COORDINATOR_HANDLED_KEYS.contains(
                                AdaptiveUiProperties.PRIMARY_BUTTON_TEXT)
                        && !AdaptiveUiDialogViewBinder.COORDINATOR_HANDLED_KEYS.contains(
                                AdaptiveUiProperties.SECONDARY_BUTTON_TEXT));
        assertTrue(
                "DESCRIPTION must be bound live by the binder",
                !AdaptiveUiDialogViewBinder.COORDINATOR_HANDLED_KEYS.contains(
                        AdaptiveUiProperties.DESCRIPTION));
    }

    @Test
    public void testAllDeclaredKeysAreListedInAllKeys() throws Exception {
        Set<PropertyKey> allKeys = new HashSet<>(Arrays.asList(AdaptiveUiProperties.ALL_KEYS));
        assertEquals(
                "ALL_KEYS must not contain duplicates",
                AdaptiveUiProperties.ALL_KEYS.length,
                allKeys.size());
        for (Field field : AdaptiveUiProperties.class.getDeclaredFields()) {
            if (!Modifier.isStatic(field.getModifiers())
                    || !PropertyKey.class.isAssignableFrom(field.getType())) {
                continue;
            }
            Object value = field.get(null);
            assertTrue("Key missing from ALL_KEYS: " + field.getName(), allKeys.contains(value));
        }
    }

    @Test
    public void testCoordinatorHandledKeysAreAllKeys() {
        Set<PropertyKey> allKeys = new HashSet<>(Arrays.asList(AdaptiveUiProperties.ALL_KEYS));
        for (PropertyKey key : AdaptiveUiDialogViewBinder.COORDINATOR_HANDLED_KEYS) {
            assertTrue(allKeys.contains(key));
        }
    }

    // ---------------------------------------------------------------------------------------
    // Creation-time mapping and live updates.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testCreationTimeMapping() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer")
                        .with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, true)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, true)
                        .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, "Content desc")
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        assertEquals("Title", dialogModel.get(ModalDialogProperties.TITLE));
        assertEquals("Footer", dialogModel.get(ModalDialogProperties.FOOTER_MESSAGE));
        assertTrue(dialogModel.get(ModalDialogProperties.TITLE_CLOSE_BUTTON_VISIBLE));
        assertEquals("Primary", dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals("Secondary", dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertTrue(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        assertTrue(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
        assertEquals("Content desc", dialogModel.get(ModalDialogProperties.CONTENT_DESCRIPTION));
        assertTrue(dialogModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        assertNotNull(dialogModel.get(ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER));
    }

    @Test
    public void testUnsetButtonsAreAbsentFromDialogModel() {
        PropertyModel model = newModelBuilder().build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        assertNull(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertNull(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertNull(dialogModel.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    @Test
    public void testLiveUpdates() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        model.set(AdaptiveUiProperties.TITLE, "New title");
        assertEquals("New title", dialogModel.get(ModalDialogProperties.TITLE));

        model.set(AdaptiveUiProperties.FOOTER_MESSAGE, "New footer");
        assertEquals("New footer", dialogModel.get(ModalDialogProperties.FOOTER_MESSAGE));

        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, true);
        assertTrue(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, false);
        assertFalse(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, true);
        assertTrue(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, false);
        assertFalse(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
    }

    // ---------------------------------------------------------------------------------------
    // Button styles table.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testButtonStyles_BothButtons() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        assertEquals(
                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
        assertFalse(dialogModel.get(ModalDialogProperties.CHANGE_CUSTOM_VIEW_OR_BUTTONS));
    }

    @Test
    public void testButtonStyles_PrimaryOnly() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary").build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        assertEquals(
                ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
        assertFalse(dialogModel.get(ModalDialogProperties.CHANGE_CUSTOM_VIEW_OR_BUTTONS));
    }

    @Test
    public void testButtonStyles_SecondaryOnly() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        assertEquals(
                ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
        assertFalse(dialogModel.get(ModalDialogProperties.CHANGE_CUSTOM_VIEW_OR_BUTTONS));
    }

    @Test
    public void testButtonStyles_NoButtons() {
        PropertyModel model = newModelBuilder().build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        assertEquals(
                ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
        assertFalse(dialogModel.get(ModalDialogProperties.CHANGE_CUSTOM_VIEW_OR_BUTTONS));
    }

    @Test
    public void testButtonStyles_EmptyTextTreatedAsUnset() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "")
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        assertEquals(
                ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
        assertNull(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    // ---------------------------------------------------------------------------------------
    // Body layout table.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testBody_DescriptionOnly() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.DESCRIPTION, "Description").build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        ArrayList<CharSequence> paragraphs =
                dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS);
        assertNotNull(paragraphs);
        assertEquals(1, paragraphs.size());
        assertEquals("Description", paragraphs.get(0));
        assertNull(dialogModel.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    @Test
    public void testBody_CustomViewOnly() {
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.CUSTOM_VIEW, customView).build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        View body = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(body);
        assertNotSame(customView, body);
        assertSame(body, customView.getParent());
        View descriptionView = body.findViewById(R.id.adaptive_ui_dialog_description);
        assertNotNull(descriptionView);
        assertEquals(View.GONE, descriptionView.getVisibility());
        assertTrue(dialogModel.get(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE));
        assertNull(dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS));
    }

    @Test
    public void testBody_CustomViewOnly_DescriptionAddedLaterIsVisible() {
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.CUSTOM_VIEW, customView).build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        View body = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(body);
        TextView descriptionView = body.findViewById(R.id.adaptive_ui_dialog_description);
        assertNotNull(descriptionView);
        assertEquals(View.GONE, descriptionView.getVisibility());

        model.set(AdaptiveUiProperties.DESCRIPTION, "Added later");

        assertEquals(View.VISIBLE, descriptionView.getVisibility());
        assertEquals("Added later", descriptionView.getText().toString());
        assertNull(dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS));
    }

    @Test
    public void testBody_DescriptionAndCustomView() {
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);
        PropertyModel dialogModel = coordinator.getDialogModel();

        View body = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(body);
        assertNotSame(customView, body);
        assertNull(dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS));
        assertTrue(dialogModel.get(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE));

        TextView descriptionView = body.findViewById(R.id.adaptive_ui_dialog_description);
        assertNotNull(descriptionView);
        assertEquals("Description", descriptionView.getText().toString());
        assertSame(body, customView.getParent());

        coordinator.destroy();
        assertNull(customView.getParent());
    }

    @Test
    public void testBody_DescriptionAndCustomView_ReparentsExistingParent() {
        ViewGroup oldParent = new android.widget.FrameLayout(mActivity);
        View customView = new View(mActivity);
        oldParent.addView(customView);
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        assertSame(dialogModel.get(ModalDialogProperties.CUSTOM_VIEW), customView.getParent());
        assertEquals(0, oldParent.getChildCount());
    }

    @Test
    public void testDestroy_DetachesCustomViewAndIsIdempotent() {
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.CUSTOM_VIEW, customView).build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);
        assertNotNull(customView.getParent());

        coordinator.destroy();
        assertNull(customView.getParent());
        coordinator.destroy();
        assertNull(customView.getParent());
    }

    // ---------------------------------------------------------------------------------------
    // Clicks and dismissal.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testPrimaryClick_RunsCallbackAndRequestsDismiss() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.POSITIVE);

        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testSecondaryClick_RunsCallbackAndRequestsDismiss() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.NEGATIVE);

        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testPrimaryClick_KeepsOpen() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.POSITIVE);
        coordinator.onClick(coordinator.getDialogModel(), ButtonType.POSITIVE);

        assertEquals(2, clicks[0]);
        assertTrue(mDialogDismissals.isEmpty());
    }

    @Test
    public void testSecondaryClick_KeepsOpen() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN, true)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.NEGATIVE);

        assertEquals(1, clicks[0]);
        assertTrue(mDialogDismissals.isEmpty());
    }

    @Test
    public void testClickWithoutCallback_StillRequestsDismiss() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary").build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.POSITIVE);

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testDismissRequestOnlyReportedOnce() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.POSITIVE);
        coordinator.onClick(coordinator.getDialogModel(), ButtonType.NEGATIVE);

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testCloseButtonClick_RequestsDismiss() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true).build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();

        dialogModel
                .get(ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER)
                .onClick(new View(mActivity));

        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDialogDismissals);
    }

    @Test
    public void testOnDismiss_DestroysChangeProcessorAndNotifiesHandler() {
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);
        PropertyModel dialogModel = coordinator.getDialogModel();

        coordinator.onDismiss(dialogModel, DialogDismissalCause.UNKNOWN);

        assertEquals(List.of(DismissReason.NAVIGATION), mDialogDismissals);
        assertNull(customView.getParent());

        model.set(AdaptiveUiProperties.TITLE, "Changed after dismiss");
        assertEquals("Title", dialogModel.get(ModalDialogProperties.TITLE));

        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, true);
        assertFalse(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    public void testOnDismiss_NotifiesHandlerOnlyOnce() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onDismiss(coordinator.getDialogModel(), DialogDismissalCause.UNKNOWN);
        coordinator.onDismiss(coordinator.getDialogModel(), DialogDismissalCause.UNKNOWN);

        assertEquals(1, mDialogDismissals.size());
    }

    @Test
    public void testDestroyBeforeDismiss_SuppressesDismissNotification() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.destroy();
        coordinator.onDismiss(coordinator.getDialogModel(), DialogDismissalCause.UNKNOWN);

        assertTrue(mDialogDismissals.isEmpty());
    }

    @Test
    public void testClicksAfterDestroyAreIgnored() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.destroy();
        coordinator.onClick(coordinator.getDialogModel(), ButtonType.POSITIVE);

        assertEquals(0, clicks[0]);
        assertTrue(mDialogDismissals.isEmpty());
    }

    // ---------------------------------------------------------------------------------------
    // Live DESCRIPTION updates.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testLiveUpdates_DescriptionWithoutCustomView() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.DESCRIPTION, "Initial").build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        assertEquals(List.of("Initial"), dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS));

        model.set(AdaptiveUiProperties.DESCRIPTION, "Updated");
        assertEquals(List.of("Updated"), dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS));

        model.set(AdaptiveUiProperties.DESCRIPTION, "");
        assertNull(dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS));
    }

    @Test
    public void testLiveUpdates_DescriptionWithCustomView() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DESCRIPTION, "Initial")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                        .build();
        PropertyModel dialogModel = createCoordinator(model).getDialogModel();
        View body = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(body);
        TextView descriptionView = body.findViewById(R.id.adaptive_ui_dialog_description);
        assertNotNull(descriptionView);
        assertEquals("Initial", descriptionView.getText().toString());
        assertEquals(View.VISIBLE, descriptionView.getVisibility());

        model.set(AdaptiveUiProperties.DESCRIPTION, "Updated");
        assertEquals("Updated", descriptionView.getText().toString());
        assertEquals(View.VISIBLE, descriptionView.getVisibility());

        model.set(AdaptiveUiProperties.DESCRIPTION, null);
        assertEquals(View.GONE, descriptionView.getVisibility());
    }

    // ---------------------------------------------------------------------------------------
    // BACK_PRESS_HANDLER scope and null guards.
    // ---------------------------------------------------------------------------------------

    private static OnBackPressedCallback newBackPressHandler(int[] handled) {
        return new OnBackPressedCallback(/* enabled= */ true) {
            @Override
            public void handleOnBackPressed() {
                handled[0]++;
            }
        };
    }

    @Test
    public void testBackPress_WindowScope_UnsetHandlerRequestsDismiss() {
        PropertyModel model = newModelBuilder().build();
        PropertyModel dialogModel =
                createCoordinator(model, AdaptiveUiScope.WINDOW).getDialogModel();

        dialogModel
                .get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER)
                .handleOnBackPressed();

        assertEquals(List.of(DismissReason.BACK_PRESS), mDialogDismissals);
    }

    @Test
    public void testBackPress_WindowScope_EnabledHandlerConsumesBackPress() {
        int[] handled = new int[1];
        OnBackPressedCallback handler = newBackPressHandler(handled);
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.BACK_PRESS_HANDLER, handler).build();
        PropertyModel dialogModel =
                createCoordinator(model, AdaptiveUiScope.WINDOW).getDialogModel();

        dialogModel
                .get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER)
                .handleOnBackPressed();

        assertEquals(1, handled[0]);
        assertTrue(mDialogDismissals.isEmpty());
    }

    @Test
    public void testBackPress_WindowScope_ClearedHandlerFallsBackToDismiss() {
        int[] handled = new int[1];
        OnBackPressedCallback handler = newBackPressHandler(handled);
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.BACK_PRESS_HANDLER, handler).build();
        PropertyModel dialogModel =
                createCoordinator(model, AdaptiveUiScope.WINDOW).getDialogModel();

        model.set(AdaptiveUiProperties.BACK_PRESS_HANDLER, null);
        dialogModel
                .get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER)
                .handleOnBackPressed();

        assertEquals(0, handled[0]);
        assertEquals(List.of(DismissReason.BACK_PRESS), mDialogDismissals);
    }

    @Test
    public void testBackPress_WindowScope_ReplacedHandlerRunsNewHandler() {
        int[] handledA = new int[1];
        int[] handledB = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.BACK_PRESS_HANDLER,
                                newBackPressHandler(handledA))
                        .build();
        PropertyModel dialogModel =
                createCoordinator(model, AdaptiveUiScope.WINDOW).getDialogModel();

        model.set(AdaptiveUiProperties.BACK_PRESS_HANDLER, newBackPressHandler(handledB));
        dialogModel
                .get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER)
                .handleOnBackPressed();

        assertEquals(0, handledA[0]);
        assertEquals(1, handledB[0]);
        assertTrue(mDialogDismissals.isEmpty());
    }

    @Test
    public void testBackPress_WindowScope_IgnoredAfterDismissRequested() {
        int[] handled = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.BACK_PRESS_HANDLER, newBackPressHandler(handled))
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model, AdaptiveUiScope.WINDOW);
        PropertyModel dialogModel = coordinator.getDialogModel();

        coordinator.onClick(dialogModel, ButtonType.POSITIVE);
        dialogModel
                .get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER)
                .handleOnBackPressed();

        assertEquals(0, handled[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testBackPress_TransientScope_DoesNotSetAppModalProperty() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.BACK_PRESS_HANDLER,
                                newBackPressHandler(new int[1]))
                        .build();
        PropertyModel dialogModel =
                createCoordinator(model, AdaptiveUiScope.TRANSIENT).getDialogModel();

        assertFalse(
                dialogModel
                        .getAllSetProperties()
                        .contains(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER));
    }

    // ---------------------------------------------------------------------------------------
    // Latched and custom dismiss reasons.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testOnDismiss_LatchedCloseButton_OverridesContainerCause() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator
                .getDialogModel()
                .get(ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER)
                .onClick(null);
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDialogDismissals);

        // Container dismisses dialog using NEGATIVE_BUTTON_CLICKED, but latched CLOSE_BUTTON takes
        // precedence.
        coordinator.onDismiss(
                coordinator.getDialogModel(), DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDialogDismissals);
    }

    @Test
    public void testOnDismiss_LatchedSecondaryAction_OverridesContainerCause() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "S").build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.onClick(coordinator.getDialogModel(), ButtonType.NEGATIVE);
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mDialogDismissals);

        // Container dismisses dialog using NEGATIVE_BUTTON_CLICKED, but latched SECONDARY_ACTION
        // takes precedence.
        coordinator.onDismiss(
                coordinator.getDialogModel(), DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testOnDismiss_LatchedBackPress_OverridesContainerCause() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model, AdaptiveUiScope.WINDOW);

        coordinator
                .getDialogModel()
                .get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER)
                .handleOnBackPressed();
        assertEquals(List.of(DismissReason.BACK_PRESS), mDialogDismissals);

        // Container dismisses dialog using NAVIGATE_BACK_OR_TOUCH_OUTSIDE, but latched BACK_PRESS
        // takes precedence.
        coordinator.onDismiss(
                coordinator.getDialogModel(), DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        assertEquals(List.of(DismissReason.BACK_PRESS), mDialogDismissals);
    }

    @Test
    public void testOnDismiss_UnlatchedTouchOutside() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model, AdaptiveUiScope.WINDOW);

        // Unlatched external tap on scrim.
        coordinator.onDismiss(
                coordinator.getDialogModel(), DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        assertEquals(List.of(DismissReason.TOUCH_OUTSIDE), mDialogDismissals);
    }

    @Test
    public void testSetDismissReason_OverridesContainerCause() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator.setDismissReason(DismissReason.PRIMARY_ACTION);
        coordinator.onDismiss(coordinator.getDialogModel(), DialogDismissalCause.UNKNOWN);

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDialogDismissals);
    }

    @Test
    public void testSetDismissReason_IgnoredIfAlreadyLatched() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        coordinator
                .getDialogModel()
                .get(ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER)
                .onClick(null);
        coordinator.setDismissReason(DismissReason.TAB_SWITCHED);
        coordinator.onDismiss(coordinator.getDialogModel(), DialogDismissalCause.UNKNOWN);

        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDialogDismissals);
    }

    @Test
    public void testToDialogDismissalCause_TransientScope() {
        AdaptiveUiDialogCoordinator coordinator =
                createCoordinator(newModelBuilder().build(), AdaptiveUiScope.TRANSIENT);
        assertEquals(
                DialogDismissalCause.NAVIGATE_BACK,
                coordinator.toDialogDismissalCause(DismissReason.BACK_PRESS));
        assertEquals(
                DialogDismissalCause.TOUCH_OUTSIDE,
                coordinator.toDialogDismissalCause(DismissReason.TOUCH_OUTSIDE));
        assertEquals(
                DialogDismissalCause.TOUCH_OUTSIDE,
                coordinator.toDialogDismissalCause(DismissReason.SWIPE_DOWN));
        assertEquals(
                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED,
                coordinator.toDialogDismissalCause(DismissReason.CLOSE_BUTTON));
        assertEquals(
                DialogDismissalCause.POSITIVE_BUTTON_CLICKED,
                coordinator.toDialogDismissalCause(DismissReason.PRIMARY_ACTION));
        assertEquals(
                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED,
                coordinator.toDialogDismissalCause(DismissReason.SECONDARY_ACTION));
        assertEquals(
                DialogDismissalCause.NAVIGATE,
                coordinator.toDialogDismissalCause(DismissReason.NAVIGATION));
        assertEquals(
                DialogDismissalCause.TAB_SWITCHED,
                coordinator.toDialogDismissalCause(DismissReason.TAB_SWITCHED));
        assertEquals(
                DialogDismissalCause.TAB_DESTROYED,
                coordinator.toDialogDismissalCause(DismissReason.TAB_CLOSED));
    }

    @Test
    public void testToDialogDismissalCause_WindowScope() {
        AdaptiveUiDialogCoordinator coordinator =
                createCoordinator(newModelBuilder().build(), AdaptiveUiScope.WINDOW);
        assertEquals(
                DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE,
                coordinator.toDialogDismissalCause(DismissReason.BACK_PRESS));
        assertEquals(
                DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE,
                coordinator.toDialogDismissalCause(DismissReason.TOUCH_OUTSIDE));
        assertEquals(
                DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE,
                coordinator.toDialogDismissalCause(DismissReason.SWIPE_DOWN));
    }

    private @DismissReason int dismissWithCause(@DialogDismissalCause int cause) {
        mDialogDismissals.clear();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(newModelBuilder().build());
        coordinator.onDismiss(coordinator.getDialogModel(), cause);
        assertEquals(1, mDialogDismissals.size());
        return mDialogDismissals.get(0);
    }

    @Test
    public void testOnDismiss_TranslatesUnlatchedDialogDismissalCauses() {
        assertEquals(
                DismissReason.BACK_PRESS, dismissWithCause(DialogDismissalCause.NAVIGATE_BACK));
        assertEquals(
                DismissReason.TOUCH_OUTSIDE, dismissWithCause(DialogDismissalCause.TOUCH_OUTSIDE));
        assertEquals(
                DismissReason.TOUCH_OUTSIDE,
                dismissWithCause(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));
        assertEquals(
                DismissReason.PRIMARY_ACTION,
                dismissWithCause(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
        assertEquals(
                DismissReason.PRIMARY_ACTION,
                dismissWithCause(DialogDismissalCause.ACTION_ON_CONTENT));
        assertEquals(
                DismissReason.PRIMARY_ACTION,
                dismissWithCause(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));
        assertEquals(
                DismissReason.SECONDARY_ACTION,
                dismissWithCause(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));
        assertEquals(
                DismissReason.SECONDARY_ACTION,
                dismissWithCause(DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE));
        assertEquals(DismissReason.NAVIGATION, dismissWithCause(DialogDismissalCause.NAVIGATE));
        assertEquals(
                DismissReason.TAB_SWITCHED, dismissWithCause(DialogDismissalCause.TAB_SWITCHED));
        assertEquals(
                DismissReason.TAB_CLOSED, dismissWithCause(DialogDismissalCause.TAB_DESTROYED));
        assertEquals(
                DismissReason.TAB_CLOSED,
                dismissWithCause(DialogDismissalCause.ACTIVITY_DESTROYED));
        assertEquals(
                DismissReason.TAB_CLOSED,
                dismissWithCause(DialogDismissalCause.NOT_ATTACHED_TO_WINDOW));
        assertEquals(
                DismissReason.TAB_CLOSED,
                dismissWithCause(DialogDismissalCause.WEB_CONTENTS_DESTROYED));
        assertEquals(DismissReason.NAVIGATION, dismissWithCause(DialogDismissalCause.UNKNOWN));
        assertEquals(
                DismissReason.NAVIGATION,
                dismissWithCause(DialogDismissalCause.DISMISSED_BY_NATIVE));
        assertEquals(
                DismissReason.NAVIGATION,
                dismissWithCause(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED));
        assertEquals(
                DismissReason.NAVIGATION, dismissWithCause(DialogDismissalCause.CLIENT_TIMEOUT));
    }

    // ---------------------------------------------------------------------------------------
    // Real ModalDialogView + ModalDialogViewBinder integration.
    // ---------------------------------------------------------------------------------------

    private ModalDialogView bindToRealModalDialogView(PropertyModel dialogModel) {
        LayoutInflater inflater =
                LayoutInflater.from(
                        new ContextThemeWrapper(
                                mActivity,
                                org.chromium.components.browser_ui.modaldialog.R.style
                                        .ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton));
        ModalDialogView dialogView =
                (ModalDialogView)
                        inflater.inflate(
                                org.chromium.components.browser_ui.modaldialog.R.layout
                                        .modal_dialog_view,
                                null);
        PropertyModelChangeProcessor.create(dialogModel, dialogView, new ModalDialogViewBinder());
        return dialogView;
    }

    @Test
    public void testRealModalDialogView_BothButtons() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        assertNotNull(bindToRealModalDialogView(coordinator.getDialogModel()));

        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "New primary");
        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "New secondary");
        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, true);
        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, true);
        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, false);
        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, false);
    }

    @Test
    public void testRealModalDialogView_PrimaryButtonOnly() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary").build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        assertNotNull(bindToRealModalDialogView(coordinator.getDialogModel()));
    }

    @Test
    public void testRealModalDialogView_SecondaryButtonOnly() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        assertNotNull(bindToRealModalDialogView(coordinator.getDialogModel()));
    }

    @Test
    public void testRealModalDialogView_NoButtons() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .build();
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(model);

        assertNotNull(bindToRealModalDialogView(coordinator.getDialogModel()));
    }

    @Test
    public void testRealModalDialogView_BodyCombinations() {
        // Description + custom view.
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                        .build();
        assertNotNull(bindToRealModalDialogView(createCoordinator(model).getDialogModel()));

        // Non-scrollable custom view only.
        model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                        .build();
        assertNotNull(bindToRealModalDialogView(createCoordinator(model).getDialogModel()));

        // Real scrollable custom view only.
        model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CUSTOM_VIEW,
                                new android.widget.ScrollView(mActivity))
                        .build();
        assertNotNull(bindToRealModalDialogView(createCoordinator(model).getDialogModel()));
    }

    // ---------------------------------------------------------------------------------------
    // Driving the ModalDialogManager.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testShow_QueuesDialogWithTypeFromScope() {
        AdaptiveUiDialogCoordinator transientDialog =
                createCoordinator(newModelBuilder().build(), AdaptiveUiScope.TRANSIENT);
        transientDialog.show();
        verify(mModalDialogManager).addObserver(transientDialog);
        verify(mModalDialogManager)
                .showDialog(transientDialog.getDialogModel(), ModalDialogType.TAB);

        AdaptiveUiDialogCoordinator windowDialog =
                createCoordinator(newModelBuilder().build(), AdaptiveUiScope.WINDOW);
        windowDialog.show();
        verify(mModalDialogManager).showDialog(windowDialog.getDialogModel(), ModalDialogType.APP);
    }

    @Test
    public void testOnShown_OnlyOnceAndOnlyForOwnDialog() {
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(newModelBuilder().build());
        coordinator.show();

        // Another dialog is shown first; this one is still queued.
        coordinator.onDialogCreated(mOtherDialogModel, /* dialog= */ null);
        coordinator.onDialogShown(new View(mActivity));
        assertEquals(0, mShownCount);

        coordinator.onDialogCreated(coordinator.getDialogModel(), /* dialog= */ null);
        coordinator.onDialogShown(new View(mActivity));
        // Suspended and shown again.
        coordinator.onDialogCreated(coordinator.getDialogModel(), /* dialog= */ null);
        coordinator.onDialogShown(new View(mActivity));
        assertEquals(1, mShownCount);
    }

    @Test
    public void testHide_DismissesWithMappedCauseAndReportsReason() {
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(newModelBuilder().build());
        coordinator.show();

        coordinator.hide(DismissReason.NAVIGATION);

        verify(mModalDialogManager)
                .dismissDialog(coordinator.getDialogModel(), DialogDismissalCause.NAVIGATE);
        assertEquals(List.of(DismissReason.NAVIGATION), mDialogDismissals);
        verify(mModalDialogManager).removeObserver(coordinator);
    }

    @Test
    public void testHide_AfterDismissalIsIgnored() {
        AdaptiveUiDialogCoordinator coordinator = createCoordinator(newModelBuilder().build());
        coordinator.show();
        coordinator.onDismiss(coordinator.getDialogModel(), DialogDismissalCause.TAB_SWITCHED);

        coordinator.hide(DismissReason.CLOSE_BUTTON);

        verify(mModalDialogManager, never())
                .dismissDialog(eq(coordinator.getDialogModel()), anyInt());
        assertEquals(List.of(DismissReason.TAB_SWITCHED), mDialogDismissals);
    }
}
