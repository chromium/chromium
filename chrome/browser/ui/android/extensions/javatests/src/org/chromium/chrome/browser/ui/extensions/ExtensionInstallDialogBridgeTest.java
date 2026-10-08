// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.textfield.TextInputEditText;

import org.jni_zero.JniUniquePtr;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.extensions.ExtensionInstallDialogBridge.Natives;
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

    public static class MyTestActivity extends Activity {}

    private static final String TITLE = "Add 'extension name'?";
    private static final String ACCEPT_BUTTON_LABEL = "Add extension";
    private static final String CANCEL_BUTTON_LABEL = "Cancel";
    private static final String PERMISSIONS_HEADING = "It can:";
    private static final String PERMISSIONS_SHOW_DETAILS = "Show Details";
    private static final String PERMISSIONS_HIDE_DETAILS = "Hide Details";
    private static final String JUSTIFICATION_HEADING =
            "Justification for requesting this extension:";
    private static final String JUSTIFICATION_PLACEHOLDER = "Enter justification...";
    private static final String JUSTIFICATION_TEXT_INPUT = "This is a test justification.";
    private static final String SITE_ACCESS_HEADING = "Allow site access";
    private static final String SITE_ACCESS_ON_CLICK = "When you click the extension";
    private static final String SITE_ACCESS_ALWAYS_ALL_SITES = "Always on all sites";
    private static final Bitmap ICON = Bitmap.createBitmap(24, 24, Bitmap.Config.ARGB_8888);

    @Mock private Natives mNativeMock;

    @Mock
    private JniUniquePtr<ExtensionInstallDialogBridge.NativeExtensionInstallDialogViewAndroid>
            mNativePtr;

    private FakeModalDialogManager mModalDialogManager;
    private Resources mResources;
    private Activity mActivity;
    private ExtensionInstallDialogBridge mExtensionInstallDialogBridge;

    @Before
    public void setUp() {
        reset(mNativeMock);
        mActivity = Robolectric.buildActivity(MyTestActivity.class).setup().get();

        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mResources = ApplicationProvider.getApplicationContext().getResources();
        ExtensionInstallDialogBridgeJni.setInstanceForTesting(mNativeMock);

        mExtensionInstallDialogBridge =
                new ExtensionInstallDialogBridge(mNativePtr, mActivity, mModalDialogManager);
    }

    /** Helper method to build and show the dialog with standard test data. */
    private void buildAndShowDialog() {
        mExtensionInstallDialogBridge.buildDialog(
                TITLE, ICON, ACCEPT_BUTTON_LABEL, CANCEL_BUTTON_LABEL);
        mExtensionInstallDialogBridge.showDialog();
    }

    /** Tests that the basic dialog only contains the title and buttons */
    @Test
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
        Assert.assertEquals(
                "Button styles do not match.",
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));

        Assert.assertNull(
                "Custom view should be null when there are not permissions.",
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    /**
     * Tests that the dialog contains the permissions container with the correct information when
     * permissions are added
     */
    @Test
    public void testDialogWithPermissions() throws Exception {
        String[] permissionsText = {"Permission #1", "Permission #2"};
        String[] permissionsDetails = {"", "Details #1"};

        mExtensionInstallDialogBridge.withPermissions(
                PERMISSIONS_HEADING,
                permissionsText,
                permissionsDetails,
                PERMISSIONS_SHOW_DETAILS,
                PERMISSIONS_HIDE_DETAILS);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();

        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        LinearLayout permissionsContainer = customView.findViewById(R.id.permissions_container);
        // Permissions container includes the heading and an entry per permission.
        int expectedChildCount = 1 + permissionsText.length;
        Assert.assertEquals(expectedChildCount, permissionsContainer.getChildCount());

        // Verify permissions heading.
        TextView headingView = customView.findViewById(R.id.permissions_heading);
        Assert.assertEquals(PERMISSIONS_HEADING, headingView.getText());

        // Verify first permission with details.
        int permissionOneIndex = 0;
        View permissionOne = permissionsContainer.getChildAt(permissionOneIndex + 1);
        TextViewWithLeading permissionOneMainText =
                permissionOne.findViewById(R.id.permission_item_text);
        TextViewWithLeading permissionOneToggle =
                permissionOne.findViewById(R.id.permission_item_toggle);
        TextViewWithLeading permissionOneDetails =
                permissionOne.findViewById(R.id.permission_item_details);
        Assert.assertEquals(
                "Permission one text does not match.",
                permissionsText[permissionOneIndex],
                permissionOneMainText.getText().toString());
        Assert.assertEquals(
                "Permission one toggle should be hidden, since permission has no details",
                View.GONE,
                permissionOneToggle.getVisibility());
        Assert.assertEquals(
                "Permission one details should be hidden, since permission has no details.",
                View.GONE,
                permissionOneDetails.getVisibility());

        // Verify second permission with no details.
        int permissionTwoIndex = 1;
        View permissionTwo = permissionsContainer.getChildAt(permissionTwoIndex + 1);
        TextViewWithLeading permissionTwoMainText =
                permissionTwo.findViewById(R.id.permission_item_text);
        TextViewWithLeading permissionTwoToggle =
                permissionTwo.findViewById(R.id.permission_item_toggle);
        TextViewWithLeading permissionTwoDetails =
                permissionTwo.findViewById(R.id.permission_item_details);
        Assert.assertEquals(
                "Permission two text does not match.",
                permissionsText[permissionTwoIndex],
                permissionTwoMainText.getText().toString());
        Assert.assertEquals(
                "Permission two toggle should be visible.",
                View.VISIBLE,
                permissionTwoToggle.getVisibility());
        Assert.assertEquals(
                "Permission two details should be initially hidden.",
                View.GONE,
                permissionTwoDetails.getVisibility());
        // Clicking on the toggle should show the details.
        permissionTwoToggle.performClick();
        Assert.assertEquals(
                "Permissions two details should be visible after toggle click.",
                View.VISIBLE,
                permissionTwoDetails.getVisibility());
        Assert.assertEquals(
                "Permission two details does not match.",
                permissionsDetails[permissionTwoIndex],
                permissionTwoDetails.getText().toString());
        Assert.assertEquals(
                "Permissions two toggle text should chang.",
                PERMISSIONS_HIDE_DETAILS,
                permissionTwoToggle.getText().toString());
        // Clicking on the toggle again should hide the details.
        permissionTwoToggle.performClick();
        Assert.assertEquals(
                "Permissions two details should be hidden after second click.",
                View.GONE,
                permissionTwoDetails.getVisibility());
        Assert.assertEquals(
                "Permissions two toggle text should revert.",
                PERMISSIONS_SHOW_DETAILS,
                permissionTwoToggle.getText().toString());
    }

    /**
     * Tests that the dialog contains the justification container and that the entered text is
     * passed to the native onDialogAccepted method.
     */
    @Test
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
                .onDialogAccepted(same(mNativePtr), eq(JUSTIFICATION_TEXT_INPUT), eq(false));
        verify(mNativePtr, times(1)).destroy();
    }

    /**
     * Tests that the positive button is disabled when the text in the justification input field
     * exceeds the maximum allowed length.
     */
    @Test
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
     * Tests that clicking on the store link calls native method when active, and does not call it
     * after the native pointer is cleared.
     */
    @Test
    public void testStoreLinkClick() throws Exception {
        String storeLinkText = "Open in Chrome Web Store";
        String storeUrl = "https://chrome.google.com/webstore/detail/";

        // Create placeholder views to avoid layout inflation issues.
        LinearLayout contentView = new LinearLayout(mActivity);
        LinearLayout infoContainer = new LinearLayout(mActivity);
        infoContainer.setId(R.id.webstore_info_container);
        contentView.addView(infoContainer);

        TextView storeLink = new TextView(mActivity);
        storeLink.setId(R.id.store_link);
        contentView.addView(storeLink);

        mExtensionInstallDialogBridge.setContentViewForTesting(contentView);

        mExtensionInstallDialogBridge.withWebstoreData(storeLinkText, "", "", 0.0, storeUrl);

        // 1. Click when active: this should call native.
        storeLink.performClick();
        verify(mNativeMock, times(1)).onStoreLinkClicked(same(mNativePtr), eq(storeUrl));

        // 2. Dismiss, which destroys and clears the pointer, then click again: this should NOT
        // call native.
        mExtensionInstallDialogBridge.onDismiss(null, DialogDismissalCause.DISMISSED_BY_NATIVE);
        reset(mNativeMock);
        storeLink.performClick();
        verify(mNativeMock, times(0)).onStoreLinkClicked(any(), any());
    }

    /** Tests that onDismiss does not call native methods after the native pointer is cleared. */
    @Test
    public void testOnDismissAfterPointerCleared() {
        // 1. Dismiss once, which destroys and clears the pointer.
        mExtensionInstallDialogBridge.onDismiss(null, DialogDismissalCause.DISMISSED_BY_NATIVE);
        reset(mNativeMock);

        // 2. Trigger onDismiss again.
        mExtensionInstallDialogBridge.onDismiss(null, DialogDismissalCause.DISMISSED_BY_NATIVE);

        // 3. Verify no JNI calls were made to onDialogDismissed or onDialogCanceled.
        verify(mNativeMock, times(0)).onDialogDismissed(any());
        verify(mNativeMock, times(0)).onDialogCanceled(any());
        verify(mNativePtr, times(1)).destroy();
    }

    /** Tests that tapjacking protections are correctly applied to the dialog model. */
    @Test
    public void testTapjackingProtections() {
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();

        Assert.assertNotNull("Dialog model should not be null", dialogModel);
        Assert.assertTrue(
                "FILTER_TOUCH_FOR_SECURITY should be true",
                dialogModel.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY));
        Assert.assertEquals(
                "BUTTON_TAP_PROTECTION_PERIOD_MS should match",
                org.chromium.ui.UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS,
                dialogModel.get(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
    }

    /**
     * Tests that clicking on the dialog's cancel button triggers the onDialogAccepted() and
     * destroy() callbacks.
     */
    @Test
    public void testOnAcceptButtonClicked() throws Exception {
        buildAndShowDialog();

        mModalDialogManager.clickPositiveButton();

        String justification = "";
        verify(mNativeMock, times(1))
                .onDialogAccepted(same(mNativePtr), eq(justification), eq(false));
        verify(mNativePtr, times(1)).destroy();
    }

    /**
     * Tests that clicking on the dialog's accept button triggers the onDialogCanceled() and
     * destroy() callbacks.
     */
    @Test
    public void testOnCancelButtonClicked() throws Exception {
        buildAndShowDialog();

        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDialogCanceled(same(mNativePtr));
        verify(mNativePtr, times(1)).destroy();
    }

    /**
     * Tests that dismissing the dialog (without clicking any of the buttons) triggers the
     * onDialogDismissed() and destroy() callbacks.
     */
    @Test
    public void testOnDialogDismissed() throws Exception {
        buildAndShowDialog();

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(model);
        mModalDialogManager.dismissDialog(model, DialogDismissalCause.UNKNOWN);

        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDialogDismissed(same(mNativePtr));
        verify(mNativePtr, times(1)).destroy();
    }

    /** Tests that the dialog displays the site access options correctly. */
    @Test
    public void testDialogWithSiteAccessOptions() throws Exception {
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                SITE_ACCESS_HEADING, SITE_ACCESS_ON_CLICK, SITE_ACCESS_ALWAYS_ALL_SITES);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        LinearLayout siteAccessContainer = customView.findViewById(R.id.site_access_container);
        Assert.assertEquals(View.VISIBLE, siteAccessContainer.getVisibility());

        TextView headingView = customView.findViewById(R.id.site_access_heading);
        Assert.assertEquals(SITE_ACCESS_HEADING, headingView.getText());

        RadioGroup radioGroup = customView.findViewById(R.id.site_access_radio_group);
        Assert.assertNotNull("Site access radio group must exist.", radioGroup);

        RadioButton onClickRadioButton = customView.findViewById(R.id.site_access_on_click);
        Assert.assertNotNull("On click radio button must exist.", onClickRadioButton);
        Assert.assertEquals(SITE_ACCESS_ON_CLICK, onClickRadioButton.getText());
        Assert.assertTrue(
                "On click radio button should be checked by default.",
                onClickRadioButton.isChecked());

        RadioButton alwaysAllSitesRadioButton =
                customView.findViewById(R.id.site_access_always_all_sites);
        Assert.assertNotNull(
                "Always all sites radio button must exist.", alwaysAllSitesRadioButton);
        Assert.assertEquals(SITE_ACCESS_ALWAYS_ALL_SITES, alwaysAllSitesRadioButton.getText());
        Assert.assertFalse(
                "Always all sites radio button should not be checked by default.",
                alwaysAllSitesRadioButton.isChecked());
    }

    /**
     * Tests that accepting the dialog with the default 'on click' option selected withholds
     * permissions.
     */
    @Test
    public void testAcceptWithSiteAccessOnClickWithholdsPermissions() throws Exception {
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                SITE_ACCESS_HEADING, SITE_ACCESS_ON_CLICK, SITE_ACCESS_ALWAYS_ALL_SITES);
        buildAndShowDialog();

        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1)).onDialogAccepted(same(mNativePtr), eq(""), eq(true));
        verify(mNativePtr, times(1)).destroy();
    }

    /**
     * Tests that accepting the dialog after selecting 'always on all sites' grants permissions
     * (does not withhold permissions).
     */
    @Test
    public void testAcceptWithSiteAccessAlwaysAllSitesGrantsPermissions() throws Exception {
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                SITE_ACCESS_HEADING, SITE_ACCESS_ON_CLICK, SITE_ACCESS_ALWAYS_ALL_SITES);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        RadioButton alwaysAllSitesRadioButton =
                customView.findViewById(R.id.site_access_always_all_sites);
        alwaysAllSitesRadioButton.performClick();

        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1)).onDialogAccepted(same(mNativePtr), eq(""), eq(false));
        verify(mNativePtr, times(1)).destroy();
    }

    /**
     * Tests that when both justification and site access options are present, accepting the dialog
     * passes both justification text and the site access choice.
     */
    @Test
    public void testDialogWithJustificationAndSiteAccessOptions() throws Exception {
        mExtensionInstallDialogBridge.withJustification(
                JUSTIFICATION_HEADING, JUSTIFICATION_PLACEHOLDER);
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                SITE_ACCESS_HEADING, SITE_ACCESS_ON_CLICK, SITE_ACCESS_ALWAYS_ALL_SITES);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        TextInputEditText justificationInputText =
                customView.findViewById(R.id.justification_input_text);
        justificationInputText.setText(JUSTIFICATION_TEXT_INPUT);

        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1))
                .onDialogAccepted(same(mNativePtr), eq(JUSTIFICATION_TEXT_INPUT), eq(true));
        verify(mNativePtr, times(1)).destroy();
    }
}
