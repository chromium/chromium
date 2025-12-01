// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.extensions.ExtensionInstallDialogBridge.Natives;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.TextViewWithLeading;

/** Unit tests for {@link ExtensionInstallDialogBridge} */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionInstallDialogBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private static final String TITLE = "Add 'extension name'?";
    private static final String ACCEPT_BUTTON_LABEL = "Add extension";
    private static final String CANCEL_BUTTON_LABEL = "Cancel";
    private static final String PERMISSIONS_HEADING = "It can:";
    private static final String[] PERMISSIONS_TEXT = {"Permission #1", "Permission #2"};
    private static final String[] PERMISSIONS_DETAILS = {"Details #1", ""};
    private static final String JUSTIFICATION_HEADING =
            "Justification for requesting this extension:";
    private static final String JUSTIFICATION_PLACEHOLDER = "Enter justification...";
    private static final String JUSTIFICATION_TEXT_INPUT = "This is a test justification.";
    private static final long NATIVE_INSTALL_EXTENSION_DIALOG_VIEW = 100L;
    private static final Bitmap ICON = Bitmap.createBitmap(24, 24, Bitmap.Config.ARGB_8888);

    @Mock private Natives mNativeMock;

    private FakeModalDialogManager mModalDialogManager;
    private Resources mResources;
    private Activity mActivity;
    private ExtensionInstallDialogBridge mExtensionInstallDialogBridge;

    @Before
    public void setUp() {
        reset(mNativeMock);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mResources = ApplicationProvider.getApplicationContext().getResources();
        ExtensionInstallDialogBridgeJni.setInstanceForTesting(mNativeMock);

        mExtensionInstallDialogBridge =
                new ExtensionInstallDialogBridge(
                        NATIVE_INSTALL_EXTENSION_DIALOG_VIEW, mActivity, mModalDialogManager);
    }

    /** Helper method to build and show the dialog with standard test data. */
    private void buildAndShowDialog() {
        mExtensionInstallDialogBridge.buildDialog(
                TITLE, ICON, ACCEPT_BUTTON_LABEL, CANCEL_BUTTON_LABEL);
        mExtensionInstallDialogBridge.showDialog();
    }

    /** Tests that the basic dialog only contains the title and buttons */
    @Test
    @SmallTest
    public void testBasicDialog() throws Exception {
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();

        Assert.assertEquals(
                "Dialog title does not match.",
                TITLE,
                dialogModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                "Positive button text does not match.",
                ACCEPT_BUTTON_LABEL,
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Negative button text does not match.",
                CANCEL_BUTTON_LABEL,
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        Assert.assertNull(
                "Custom view should be null when there are not permissions.",
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    /**
     * Tests that the dialog contains the permissions container with the correct information when
     * permissions are added
     */
    @Test
    @SmallTest
    public void testDialogWithPermissions() throws Exception {
        mExtensionInstallDialogBridge.withPermissions(
                PERMISSIONS_HEADING, PERMISSIONS_TEXT, PERMISSIONS_DETAILS);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();

        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        LinearLayout permissionsContainer = customView.findViewById(R.id.permissions_container);
        // Permissions container includes the heading and an entry per permission.
        int expectedChildCount = 1 + PERMISSIONS_TEXT.length;
        Assert.assertEquals(expectedChildCount, permissionsContainer.getChildCount());

        TextView headingView = customView.findViewById(R.id.permissions_heading);
        Assert.assertEquals(PERMISSIONS_HEADING, headingView.getText());

        for (int i = 0; i < PERMISSIONS_TEXT.length; i++) {
            // Permissions text start at index 1 in the container, since the heading is at index 0.
            View childView = permissionsContainer.getChildAt(i + 1);
            Assert.assertTrue(childView instanceof TextViewWithLeading);
            TextViewWithLeading permissionTextView = (TextViewWithLeading) childView;

            Assert.assertEquals(PERMISSIONS_TEXT[i], permissionTextView.getText().toString());
        }
    }

    /**
     * Tests that the dialog contains the justification container and that the entered text is
     * passed to the native onDialogAccepted method.
     */
    @Test
    @SmallTest
    public void testDialogWithJustificationAndAcceptsText() throws Exception {
        mExtensionInstallDialogBridge.withJustification(
                JUSTIFICATION_HEADING, JUSTIFICATION_PLACEHOLDER);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        // Assert the justifications container visibility and heading text.
        LinearLayout justificationContainer = customView.findViewById(R.id.justification_container);
        Assert.assertEquals(View.VISIBLE, justificationContainer.getVisibility());
        TextView headingView = customView.findViewById(R.id.justification_heading);
        Assert.assertEquals(JUSTIFICATION_HEADING, headingView.getText());

        // Simulate text entry into the input field.
        // Note: The ID here should match the ID of the inner TextInputEditText
        // which we named R.id.justification_input in the XML setup steps.
        TextInputEditText justificationInputText =
                customView.findViewById(R.id.justification_input_text);
        Assert.assertNotNull("Justification input field must exist.", justificationInputText);
        justificationInputText.setText(JUSTIFICATION_TEXT_INPUT);

        // Click the positive button (Accept).
        mModalDialogManager.clickPositiveButton();

        // Verify the native method was called with the input text.
        verify(mNativeMock, times(1))
                .onDialogAccepted(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW, JUSTIFICATION_TEXT_INPUT);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }

    /**
     * Tests that the positive button is disabled when the text in the justification input field
     * exceeds the maximum allowed length.
     */
    @Test
    @SmallTest
    public void testPositiveButtonDisabledWhenJustificationTextIsTooLong() throws Exception {
        mExtensionInstallDialogBridge.withJustification(
                JUSTIFICATION_HEADING, JUSTIFICATION_PLACEHOLDER);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        TextInputEditText justificationInputText =
                customView.findViewById(R.id.justification_input_text);

        // Get the max input length resource value.
        int maxInputLength =
                mResources.getInteger(R.integer.extension_install_dialog_justification_max_input);

        // Create a text string that is one character longer than the max.
        String tooLongText = "A".repeat(maxInputLength) + "B";

        // Verify the positive button is initially enabled (before text entry).
        Assert.assertFalse(
                "Positive button should be enabled before text entry.",
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        // Simulate text entry into the input field with the too-long string.
        justificationInputText.setText(tooLongText);

        // Assert that the positive button is now disabled.
        Assert.assertTrue(
                "Positive button must be disabled when text exceeds max length.",
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        // Test enabling the button again by shortening the text.
        String validText = tooLongText.substring(0, maxInputLength);
        justificationInputText.setText(validText);

        // Assert that the positive button is re-enabled.
        Assert.assertFalse(
                "Positive button must be re-enabled when text is shortened to max length.",
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    /**
     * Tests that clicking on the dialog's cancel button triggers the onDialogAccepted() and
     * destroy() callbacks.
     */
    @Test
    @SmallTest
    public void testOnAcceptButtonClicked() throws Exception {
        buildAndShowDialog();

        mModalDialogManager.clickPositiveButton();

        String justification = "";
        verify(mNativeMock, times(1))
                .onDialogAccepted(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW, justification);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }

    /**
     * Tests that clicking on the dialog's accept button triggers the onDialogCanceled() and
     * destroy() callbacks.
     */
    @Test
    @SmallTest
    public void testOnCancelButtonClicked() throws Exception {
        buildAndShowDialog();

        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDialogCanceled(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }

    /**
     * Tests that dismissing the dialog (without clicking any of the buttons) triggers the
     * onDialogDismissed() and destroy() callbacks.
     */
    @Test
    @SmallTest
    public void testOnDialogDismissed() throws Exception {
        buildAndShowDialog();

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(model);
        mModalDialogManager.dismissDialog(model, DialogDismissalCause.UNKNOWN);

        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDialogDismissed(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }
}
