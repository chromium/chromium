// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.Mockito.never;

import android.graphics.Bitmap;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit test for MessageWrapper. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class MessageWrapperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MessageWrapper.Natives mNativeMock;

    private JniRawPtr<MessageWrapper.NativeMessageWrapper> mNativePtr;
    private MessageWrapper mMessage;

    @Before
    public void setUp() {
        MessageWrapperJni.setInstanceForTesting(mNativeMock);
        mNativePtr = JniRawPtr.createForTesting(1L);
        mMessage = MessageWrapper.create(mNativePtr, MessageIdentifier.TEST_MESSAGE);
    }

    @After
    public void tearDown() {
        mMessage.clearNativePtr();
    }

    /** Tests that message properties are correctly propagated to PropertyModel. */
    @Test
    public void testMessageProperties() {
        PropertyModel messageProperties = mMessage.getMessageProperties();

        mMessage.setTitle("Title");
        Assert.assertEquals(
                "Title doesn't match provided value",
                "Title",
                messageProperties.get(MessageBannerProperties.TITLE));

        mMessage.setDescription("Description");
        Assert.assertEquals(
                "Description doesn't match provided value",
                "Description",
                messageProperties.get(MessageBannerProperties.DESCRIPTION));

        mMessage.setDescriptionMaxLines(2);
        Assert.assertEquals(
                "Description max lines doesn't match provided value",
                2,
                messageProperties.get(MessageBannerProperties.DESCRIPTION_MAX_LINES));

        mMessage.setPrimaryButtonText("Primary button");
        Assert.assertEquals(
                "Button text doesn't match provided value",
                "Primary button",
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));

        mMessage.setPrimaryButtonTextMaxLines(1);
        Assert.assertEquals(
                "Button text max lines doesn't match provided value",
                1,
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT_MAX_LINES));

        mMessage.setSecondaryButtonMenuText("Secondary button");
        Assert.assertEquals(
                "Button text doesn't match provided value",
                "Secondary button",
                messageProperties.get(MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT));

        mMessage.setIconResourceId(1);
        Assert.assertEquals(
                "Icon resource id doesn't match provided value",
                1,
                messageProperties.get(MessageBannerProperties.ICON_RESOURCE_ID));

        mMessage.setSecondaryIconResourceId(2);
        Assert.assertEquals(
                "Icon resource id doesn't match provided value",
                2,
                messageProperties.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID));

        mMessage.setSecondaryIconContentDescription("Secondary icon desc");
        Assert.assertEquals(
                "Secondary icon content description doesn't match provided value",
                "Secondary icon desc",
                messageProperties.get(MessageBannerProperties.SECONDARY_ICON_CONTENT_DESCRIPTION));

        Assert.assertNull(
                "Initially icon should not be set",
                messageProperties.get(MessageBannerProperties.ICON));
        mMessage.setIcon(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888));
        Assert.assertNotNull(
                "Call to setIcon() didn't update ICON property",
                messageProperties.get(MessageBannerProperties.ICON));
    }

    /** Tests that native functions are called in response to callbacks invocation. */
    @Test
    public void testCallbacks() {
        PropertyModel messageProperties = mMessage.getMessageProperties();
        var _ = messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        Mockito.verify(mNativeMock).handleActionClick(mNativePtr);
        messageProperties.get(MessageBannerProperties.ON_SECONDARY_ACTION).run();
        Mockito.verify(mNativeMock).handleSecondaryActionClick(mNativePtr);
        messageProperties
                .get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        Mockito.verify(mNativeMock).handleDismissCallback(mNativePtr, DismissReason.PRIMARY_ACTION);
    }

    /** Tests that native callbacks are not delivered if the MessageWrapper was destroyed. */
    @Test
    public void testDestroyedMessageWrapperCallbacks() {
        PropertyModel messageProperties = mMessage.getMessageProperties();

        mMessage.clearNativePtr();
        var _ = messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        Mockito.verify(mNativeMock, never()).handleActionClick(Mockito.any());
        messageProperties.get(MessageBannerProperties.ON_SECONDARY_ACTION).run();
        Mockito.verify(mNativeMock, never()).handleSecondaryActionClick(Mockito.any());
        messageProperties
                .get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        Mockito.verify(mNativeMock, never()).handleDismissCallback(Mockito.any(), Mockito.anyInt());
    }

    /**
     * Tests the secondary menu functionality including addition, selection and clearance of items.
     */
    @Test
    public void testSecondaryMenuUpdates() {

        mMessage.setTitle("Title");
        mMessage.setSecondaryIconResourceId(2);
        mMessage.setPrimaryButtonText("Primary button");

        // Add secondary menu items.
        PropertyModel item1 = mMessage.addSecondaryMenuItem(1, 0, "Item 1");
        mMessage.addSecondaryMenuItemDivider();
        mMessage.addSecondaryMenuItem(2, 0, "Item 2");
        MessageSecondaryMenuItems messageSecondaryMenuItems =
                mMessage.getMessageSecondaryMenuItemsForTesting();
        Assert.assertNotNull(messageSecondaryMenuItems);
        Assert.assertEquals(
                "Size of secondary menu does not match.",
                3,
                messageSecondaryMenuItems.mMenuItems.size());

        // Select a secondary menu item.
        mMessage.onItemSelected(item1, null);
        Mockito.verify(mNativeMock).handleSecondaryMenuItemSelected(mNativePtr, 1);

        // Clear the secondary menu.
        mMessage.clearSecondaryMenuItems();
        Assert.assertEquals(
                "Secondary menu is not cleared.", 0, messageSecondaryMenuItems.mMenuItems.size());
    }

    /** Tests that setPrimaryWidgetAppearance updates the message's primary widget appearance. */
    @Test
    public void testSetPrimaryWidgetAppearance() {
        PropertyModel messageProperties = mMessage.getMessageProperties();

        Assert.assertEquals(
                PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET,
                mMessage.getPrimaryWidgetAppearance());

        mMessage.setPrimaryWidgetAppearance(PrimaryWidgetAppearance.PROGRESS_SPINNER);

        Assert.assertEquals(
                PrimaryWidgetAppearance.PROGRESS_SPINNER, mMessage.getPrimaryWidgetAppearance());
        Assert.assertEquals(
                PrimaryWidgetAppearance.PROGRESS_SPINNER,
                messageProperties.get(MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE));
    }
}
