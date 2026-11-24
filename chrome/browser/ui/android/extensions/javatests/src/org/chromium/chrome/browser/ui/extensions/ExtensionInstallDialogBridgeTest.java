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
import android.view.accessibility.AccessibilityNodeInfo;
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
    private static final String PERMISSIONS_SHOW_DETAILS = "Show all";
    private static final String PERMISSIONS_HIDE_DETAILS = "Show less";
    private static final String JUSTIFICATION_HEADING =
            "Justification for requesting this extension:";
    private static final String JUSTIFICATION_PLACEHOLDER = "Enter justification...";
    private static final String JUSTIFICATION_TEXT_INPUT = "This is a test justification.";
    private static final String SITE_ACCESS_HEADER = "Read and change your data";
    private static final String SITE_ACCESS_ON_CLICK = "Only when you click the extension";
    private static final String SITE_ACCESS_ON_CLICK_SUBTEXT =
            "Grants temporary access to your data on the site";
    private static final String SITE_ACCESS_ALWAYS_ALL_SITES = "Always on all sites";
    private static final String SITE_ACCESS_ALWAYS_ALL_SITES_SUBTEXT =
            "Grants permanent access to your data on all sites";
    private static final String SITE_ACCESS_ALWAYS_LISTED_SITES = "Always on the listed sites";
    private static final String SITE_ACCESS_ALWAYS_LISTED_SITES_SUBTEXT =
            "Grants permanent access to your data on the sites listed below";
    private static final String SHOW_ALL_SITES_ACCESSIBLE = "Show all sites";
    private static final String LISTED_SITES_ACCESSIBLE = "Listed sites";
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
        // Parallel arrays where each index i across the three arrays defines a separate
        // permission scenario tested together in this dialog:
        String[] permissionsText = {"Permission #1", "Permission #2", "Permission #3"};
        String[] permissionsVisibleDetails = {"", "Visible Details #1", "Visible Details #2"};
        String[] permissionsDetails = {"", "Details #1", ""};

        mExtensionInstallDialogBridge.withPermissions(
                PERMISSIONS_HEADING,
                permissionsText,
                permissionsVisibleDetails,
                permissionsDetails,
                PERMISSIONS_SHOW_DETAILS,
                PERMISSIONS_HIDE_DETAILS,
                /* showBulletPoints= */ false);
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
        TextViewWithLeading permissionOneBullet =
                permissionOne.findViewById(R.id.permission_item_bullet);
        TextViewWithLeading permissionOneMainText =
                permissionOne.findViewById(R.id.permission_item_text);
        TextViewWithLeading permissionOneVisibleDetails =
                permissionOne.findViewById(R.id.permission_item_visible_details);
        TextViewWithLeading permissionOneToggle =
                permissionOne.findViewById(R.id.permission_item_toggle);
        TextViewWithLeading permissionOneDetails =
                permissionOne.findViewById(R.id.permission_item_details);
        Assert.assertEquals(
                "Permission one bullet should be hidden when showBulletPoints is false.",
                View.GONE,
                permissionOneBullet.getVisibility());
        Assert.assertEquals(
                "Permission one text does not match.",
                permissionsText[permissionOneIndex],
                permissionOneMainText.getText().toString());
        Assert.assertEquals(
                "Permission one visible details should be hidden.",
                View.GONE,
                permissionOneVisibleDetails.getVisibility());
        Assert.assertEquals(
                "Permission one toggle should be hidden, since permission has no details",
                View.GONE,
                permissionOneToggle.getVisibility());
        Assert.assertEquals(
                "Permission one details should be hidden, since permission has no details.",
                View.GONE,
                permissionOneDetails.getVisibility());

        // Verify second permission with details.
        int permissionTwoIndex = 1;
        View permissionTwo = permissionsContainer.getChildAt(permissionTwoIndex + 1);
        TextViewWithLeading permissionTwoMainText =
                permissionTwo.findViewById(R.id.permission_item_text);
        TextViewWithLeading permissionTwoVisibleDetails =
                permissionTwo.findViewById(R.id.permission_item_visible_details);
        TextViewWithLeading permissionTwoToggle =
                permissionTwo.findViewById(R.id.permission_item_toggle);
        TextViewWithLeading permissionTwoDetails =
                permissionTwo.findViewById(R.id.permission_item_details);
        Assert.assertEquals(
                "Permission two text does not match.",
                permissionsText[permissionTwoIndex],
                permissionTwoMainText.getText().toString());
        Assert.assertEquals(
                "Permission two visible details should be visible.",
                View.VISIBLE,
                permissionTwoVisibleDetails.getVisibility());
        Assert.assertEquals(
                "Permission two visible details does not match.",
                permissionsVisibleDetails[permissionTwoIndex],
                permissionTwoVisibleDetails.getText().toString());
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

        // Verify third permission with details.
        int permissionThreeIndex = 2;
        View permissionThree = permissionsContainer.getChildAt(permissionThreeIndex + 1);
        TextViewWithLeading permissionThreeMainText =
                permissionThree.findViewById(R.id.permission_item_text);
        TextViewWithLeading permissionThreeVisibleDetails =
                permissionThree.findViewById(R.id.permission_item_visible_details);
        TextViewWithLeading permissionThreeToggle =
                permissionThree.findViewById(R.id.permission_item_toggle);
        TextViewWithLeading permissionThreeDetails =
                permissionThree.findViewById(R.id.permission_item_details);
        Assert.assertEquals(
                "Permission three text does not match.",
                permissionsText[permissionThreeIndex],
                permissionThreeMainText.getText().toString());
        Assert.assertEquals(
                "Permission three visible details should be visible.",
                View.VISIBLE,
                permissionThreeVisibleDetails.getVisibility());
        Assert.assertEquals(
                "Permission three visible details does not match.",
                permissionsVisibleDetails[permissionThreeIndex],
                permissionThreeVisibleDetails.getText().toString());
        Assert.assertEquals(
                "Permission three toggle should be hidden, since permission has no collapsed"
                    + " details.",
                View.GONE,
                permissionThreeToggle.getVisibility());
        Assert.assertEquals(
                "Permission three details should be hidden, since permission has no collapsed"
                    + " details.",
                View.GONE,
                permissionThreeDetails.getVisibility());
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

    private void withAllSitesAccessOptions() {
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                PERMISSIONS_HEADING,
                SITE_ACCESS_HEADER,
                SITE_ACCESS_ON_CLICK,
                SITE_ACCESS_ON_CLICK_SUBTEXT,
                SITE_ACCESS_ALWAYS_ALL_SITES,
                SITE_ACCESS_ALWAYS_ALL_SITES_SUBTEXT,
                /* visibleHosts= */ new String[0],
                /* collapsedHosts= */ new String[0],
                PERMISSIONS_SHOW_DETAILS,
                SHOW_ALL_SITES_ACCESSIBLE,
                PERMISSIONS_HIDE_DETAILS,
                LISTED_SITES_ACCESSIBLE);
    }

    /** Tests that the dialog displays the all-sites access options under "It can:" correctly. */
    @Test
    public void testDialogWithSiteAccessOptionsAllSites() throws Exception {
        withAllSitesAccessOptions();
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        LinearLayout permissionsContainer = customView.findViewById(R.id.permissions_container);
        Assert.assertEquals(View.VISIBLE, permissionsContainer.getVisibility());
        TextView permissionsHeadingView = customView.findViewById(R.id.permissions_heading);
        Assert.assertEquals(PERMISSIONS_HEADING, permissionsHeadingView.getText());

        LinearLayout siteAccessContainer = customView.findViewById(R.id.site_access_container);
        Assert.assertSame(siteAccessContainer, permissionsContainer.getChildAt(1));
        Assert.assertEquals(View.VISIBLE, siteAccessContainer.getVisibility());

        TextView headingBulletView = customView.findViewById(R.id.site_access_heading_bullet);
        Assert.assertEquals("• ", headingBulletView.getText().toString());
        TextView headingView = customView.findViewById(R.id.site_access_heading);
        Assert.assertEquals(SITE_ACCESS_HEADER, headingView.getText().toString());

        RadioGroup radioGroup = customView.findViewById(R.id.site_access_radio_group);
        Assert.assertNotNull("Site access radio group must exist.", radioGroup);
        Assert.assertEquals(SITE_ACCESS_HEADER, radioGroup.getContentDescription().toString());

        RadioButton onClickRadioButton = customView.findViewById(R.id.site_access_on_click);
        Assert.assertNotNull("On click radio button must exist.", onClickRadioButton);
        Assert.assertEquals(SITE_ACCESS_ON_CLICK, onClickRadioButton.getText().toString());
        Assert.assertTrue(
                "On click radio button should be checked by default.",
                onClickRadioButton.isChecked());
        Assert.assertEquals(
                SITE_ACCESS_ON_CLICK + "\n" + SITE_ACCESS_ON_CLICK_SUBTEXT,
                onClickRadioButton.getContentDescription().toString());

        TextView onClickSubtext = customView.findViewById(R.id.site_access_on_click_subtext);
        Assert.assertEquals(SITE_ACCESS_ON_CLICK_SUBTEXT, onClickSubtext.getText().toString());

        RadioButton alwaysRadioButton = customView.findViewById(R.id.site_access_always);
        Assert.assertNotNull("Always radio button must exist.", alwaysRadioButton);
        Assert.assertEquals(SITE_ACCESS_ALWAYS_ALL_SITES, alwaysRadioButton.getText().toString());
        Assert.assertFalse(
                "Always radio button should not be checked by default.",
                alwaysRadioButton.isChecked());
        Assert.assertEquals(
                SITE_ACCESS_ALWAYS_ALL_SITES + "\n" + SITE_ACCESS_ALWAYS_ALL_SITES_SUBTEXT,
                alwaysRadioButton.getContentDescription().toString());

        TextView alwaysSubtext = customView.findViewById(R.id.site_access_always_subtext);
        Assert.assertEquals(
                SITE_ACCESS_ALWAYS_ALL_SITES_SUBTEXT, alwaysSubtext.getText().toString());

        LinearLayout hostsContainer = customView.findViewById(R.id.site_access_hosts_container);
        Assert.assertEquals(View.GONE, hostsContainer.getVisibility());
    }

    private static void assertSiteColumnDomains(LinearLayout column, String... expectedDomains) {
        Assert.assertEquals(expectedDomains.length, column.getChildCount());
        for (int i = 0; i < expectedDomains.length; i++) {
            View row = column.getChildAt(i);
            TextViewWithLeading bulletView = row.findViewById(R.id.site_access_item_bullet);
            TextViewWithLeading domainView = row.findViewById(R.id.site_access_item_domain);
            Assert.assertEquals("• ", bulletView.getText().toString());
            Assert.assertEquals(expectedDomains[i], domainView.getText().toString());
            Assert.assertEquals(expectedDomains[i], row.getContentDescription().toString());
        }
    }

    /** Tests that 1-3 listed sites show the bulleted host list without a "Show all" toggle. */
    @Test
    public void testDialogWithSiteAccessOptionsFewListedSites() throws Exception {
        String[] visibleHosts = {"a.com", "b.com"};
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                PERMISSIONS_HEADING,
                SITE_ACCESS_HEADER,
                SITE_ACCESS_ON_CLICK,
                SITE_ACCESS_ON_CLICK_SUBTEXT,
                SITE_ACCESS_ALWAYS_LISTED_SITES,
                SITE_ACCESS_ALWAYS_LISTED_SITES_SUBTEXT,
                visibleHosts,
                /* collapsedHosts= */ new String[0],
                PERMISSIONS_SHOW_DETAILS,
                SHOW_ALL_SITES_ACCESSIBLE,
                PERMISSIONS_HIDE_DETAILS,
                LISTED_SITES_ACCESSIBLE);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        RadioButton alwaysRadioButton = customView.findViewById(R.id.site_access_always);
        Assert.assertEquals(
                SITE_ACCESS_ALWAYS_LISTED_SITES, alwaysRadioButton.getText().toString());
        Assert.assertEquals(
                SITE_ACCESS_ALWAYS_LISTED_SITES
                        + "\n"
                        + SITE_ACCESS_ALWAYS_LISTED_SITES_SUBTEXT
                        + "\na.com\nb.com",
                alwaysRadioButton.getContentDescription().toString());

        LinearLayout hostsContainer = customView.findViewById(R.id.site_access_hosts_container);
        Assert.assertEquals(View.VISIBLE, hostsContainer.getVisibility());
        Assert.assertEquals(
                LISTED_SITES_ACCESSIBLE, hostsContainer.getContentDescription().toString());

        LinearLayout visibleHostsContainer =
                customView.findViewById(R.id.site_access_visible_hosts);
        Assert.assertEquals(View.VISIBLE, visibleHostsContainer.getVisibility());
        assertSiteColumnDomains(visibleHostsContainer, "a.com", "b.com");

        LinearLayout expandedHostsContainer =
                customView.findViewById(R.id.site_access_expanded_hosts);
        TextViewWithLeading toggleView = customView.findViewById(R.id.site_access_hosts_toggle);
        Assert.assertEquals(View.GONE, expandedHostsContainer.getVisibility());
        Assert.assertEquals(View.GONE, toggleView.getVisibility());
    }

    /**
     * Tests that >3 listed sites show 1 column of 3 sites when collapsed, and expand into a
     * 2-column layout when "Show all" is clicked or activated via {@code ACTION_EXPAND}/{@code
     * ACTION_COLLAPSE} without changing the radio selection.
     */
    @Test
    public void testDialogWithSiteAccessOptionsExpandableListedSites() throws Exception {
        String[] visibleHosts = {"a.com", "b.com", "c.com"};
        String[] collapsedHosts = {"d.com", "e.com"};
        mExtensionInstallDialogBridge.withSiteAccessOptions(
                PERMISSIONS_HEADING,
                SITE_ACCESS_HEADER,
                SITE_ACCESS_ON_CLICK,
                SITE_ACCESS_ON_CLICK_SUBTEXT,
                SITE_ACCESS_ALWAYS_LISTED_SITES,
                SITE_ACCESS_ALWAYS_LISTED_SITES_SUBTEXT,
                visibleHosts,
                collapsedHosts,
                PERMISSIONS_SHOW_DETAILS,
                SHOW_ALL_SITES_ACCESSIBLE,
                PERMISSIONS_HIDE_DETAILS,
                LISTED_SITES_ACCESSIBLE);
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        RadioButton onClickRadioButton = customView.findViewById(R.id.site_access_on_click);
        RadioButton alwaysRadioButton = customView.findViewById(R.id.site_access_always);
        LinearLayout visibleHostsContainer =
                customView.findViewById(R.id.site_access_visible_hosts);
        LinearLayout expandedHostsContainer =
                customView.findViewById(R.id.site_access_expanded_hosts);
        LinearLayout expandedColumn1 = customView.findViewById(R.id.site_access_hosts_column_1);
        LinearLayout expandedColumn2 = customView.findViewById(R.id.site_access_hosts_column_2);
        TextViewWithLeading toggleView = customView.findViewById(R.id.site_access_hosts_toggle);

        Assert.assertEquals(View.VISIBLE, visibleHostsContainer.getVisibility());
        assertSiteColumnDomains(visibleHostsContainer, "a.com", "b.com", "c.com");
        Assert.assertEquals(View.GONE, expandedHostsContainer.getVisibility());
        Assert.assertEquals(View.VISIBLE, toggleView.getVisibility());
        Assert.assertEquals(PERMISSIONS_SHOW_DETAILS, toggleView.getText().toString());
        Assert.assertEquals(
                SHOW_ALL_SITES_ACCESSIBLE, toggleView.getContentDescription().toString());

        AccessibilityNodeInfo collapsedNodeInfo = toggleView.createAccessibilityNodeInfo();
        Assert.assertTrue(
                collapsedNodeInfo
                        .getActionList()
                        .contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND));
        Assert.assertFalse(
                toggleView.performAccessibilityAction(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE.getId(), null));

        // Expand via ACTION_EXPAND: 1-column visible list hides, 2-column expanded list appears
        // with ceil(5/2)=3 sites in column 1 and 2 sites in column 2, toggle updates to
        // "Show less", and radio selection remains on "Only when you click the extension".
        Assert.assertTrue(
                toggleView.performAccessibilityAction(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND.getId(), null));
        Assert.assertEquals(View.GONE, visibleHostsContainer.getVisibility());
        Assert.assertEquals(View.VISIBLE, expandedHostsContainer.getVisibility());
        assertSiteColumnDomains(expandedColumn1, "a.com", "b.com", "c.com");
        assertSiteColumnDomains(expandedColumn2, "d.com", "e.com");
        Assert.assertEquals(PERMISSIONS_HIDE_DETAILS, toggleView.getText().toString());
        Assert.assertEquals(
                PERMISSIONS_HIDE_DETAILS, toggleView.getContentDescription().toString());
        Assert.assertEquals(
                SITE_ACCESS_ALWAYS_LISTED_SITES
                        + "\n"
                        + SITE_ACCESS_ALWAYS_LISTED_SITES_SUBTEXT
                        + "\na.com\nb.com\nc.com\nd.com\ne.com",
                alwaysRadioButton.getContentDescription().toString());
        Assert.assertTrue(onClickRadioButton.isChecked());
        Assert.assertFalse(alwaysRadioButton.isChecked());

        AccessibilityNodeInfo expandedNodeInfo = toggleView.createAccessibilityNodeInfo();
        Assert.assertTrue(
                expandedNodeInfo
                        .getActionList()
                        .contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE));
        Assert.assertFalse(
                toggleView.performAccessibilityAction(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND.getId(), null));

        // Collapse via ACTION_COLLAPSE: 2-column container hides and 1-column container returns.
        Assert.assertTrue(
                toggleView.performAccessibilityAction(
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE.getId(), null));
        Assert.assertEquals(View.VISIBLE, visibleHostsContainer.getVisibility());
        Assert.assertEquals(View.GONE, expandedHostsContainer.getVisibility());
        Assert.assertEquals(PERMISSIONS_SHOW_DETAILS, toggleView.getText().toString());
        Assert.assertEquals(
                SHOW_ALL_SITES_ACCESSIBLE, toggleView.getContentDescription().toString());
    }

    /**
     * Tests that site access options appear at the top of "It can:" (index 1) above non-host
     * permissions even when {@code withPermissions} is called before {@code withSiteAccessOptions}.
     */
    @Test
    public void testDialogWithPermissionsAndSiteAccessOptionsOrdering() throws Exception {
        String[] nonHostPermissions = {"Read your browsing history"};
        String[] emptyDetails = {""};
        mExtensionInstallDialogBridge.withPermissions(
                PERMISSIONS_HEADING,
                nonHostPermissions,
                emptyDetails,
                emptyDetails,
                PERMISSIONS_SHOW_DETAILS,
                PERMISSIONS_HIDE_DETAILS,
                /* showBulletPoints= */ true);
        withAllSitesAccessOptions();
        buildAndShowDialog();

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        LinearLayout permissionsContainer = customView.findViewById(R.id.permissions_container);

        Assert.assertEquals(3, permissionsContainer.getChildCount());
        Assert.assertEquals(R.id.permissions_heading, permissionsContainer.getChildAt(0).getId());
        Assert.assertEquals(R.id.site_access_container, permissionsContainer.getChildAt(1).getId());
        View nonHostRow = permissionsContainer.getChildAt(2);
        TextViewWithLeading nonHostBullet = nonHostRow.findViewById(R.id.permission_item_bullet);
        TextViewWithLeading nonHostPermissionText =
                nonHostRow.findViewById(R.id.permission_item_text);
        Assert.assertEquals(View.VISIBLE, nonHostBullet.getVisibility());
        Assert.assertEquals("• ", nonHostBullet.getText().toString());
        Assert.assertEquals(nonHostPermissions[0], nonHostPermissionText.getText().toString());
    }

    /**
     * Tests that accepting the dialog with the default 'on click' option selected withholds
     * permissions.
     */
    @Test
    public void testAcceptWithSiteAccessOnClickWithholdsPermissions() throws Exception {
        withAllSitesAccessOptions();
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
        withAllSitesAccessOptions();
        buildAndShowDialog();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        RadioButton onClickRadioButton = customView.findViewById(R.id.site_access_on_click);
        RadioButton alwaysRadioButton = customView.findViewById(R.id.site_access_always);
        alwaysRadioButton.performClick();
        Assert.assertFalse(onClickRadioButton.isChecked());
        Assert.assertTrue(alwaysRadioButton.isChecked());

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
        withAllSitesAccessOptions();
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
