// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.Mockito.never;

import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit test for MessageWrapper. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageWrapperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private MessageWrapper.Natives mNativeMock;

    @Before
    public void setUp() {
        mJniMocker.mock(MessageWrapperJni.TEST_HOOKS, mNativeMock);
    }

    /** Tests that message properties are correctly propagated to PropertyModel. */
    @Test
    @SmallTest
    public void testMessageProperties() {
        MessageWrapper message = MessageWrapper.create(1, MessageIdentifier.TEST_MESSAGE);
        PropertyModel messageProperties = message.getMessageProperties();

        message.setTitle("Title");
        Assert.assertEquals(
                "Title doesn't match provided value",
                "Title",
                messageProperties.get(MessageBannerProperties.TITLE));

        message.setDescription("Description");
        Assert.assertEquals(
                "Description doesn't match provided value",
                "Description",
                messageProperties.get(MessageBannerProperties.DESCRIPTION));

        message.setDescriptionMaxLines(2);
        Assert.assertEquals(
                "Description max lines doesn't match provided value",
                2,
                messageProperties.get(MessageBannerProperties.DESCRIPTION_MAX_LINES));

        message.setPrimaryButtonText("Primary button");
        Assert.assertEquals(
                "Button text doesn't match provided value",
                "Primary button",
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));

        message.setPrimaryButtonTextMaxLines(1);
        Assert.assertEquals(
                "Button text max lines doesn't match provided value",
                1,
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT_MAX_LINES));

        message.setSecondaryButtonMenuText("Secondary button");
        Assert.assertEquals(
                "Button text doesn't match provided value",
                "Secondary button",
                messageProperties.get(MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT));

        message.setIconResourceId(1);
        Assert.assertEquals(
                "Icon resource id doesn't match provided value",
                1,
                messageProperties.get(MessageBannerProperties.ICON_RESOURCE_ID));

        message.setSecondaryIconResourceId(2);
        Assert.assertEquals(
                "Icon resource id doesn't match provided value",
                2,
                messageProperties.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID));

        Assert.assertNull(
                "Initially icon should not be set",
                messageProperties.get(MessageBannerProperties.ICON));
        message.setIcon(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888));
        Assert.assertNotNull(
                "Call to setIcon() didn't update ICON property",
                messageProperties.get(MessageBannerProperties.ICON));
    }

    /** Tests that native functions are called in response to callbacks invocation. */
    @Test
    @SmallTest
    public void testCallbacks() {
        final long nativePtr = 1;
        MessageWrapper message = MessageWrapper.create(nativePtr, MessageIdentifier.TEST_MESSAGE);
        PropertyModel messageProperties = message.getMessageProperties();
        messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        Mockito.verify(mNativeMock).handleActionClick(nativePtr);
        messageProperties.get(MessageBannerProperties.ON_SECONDARY_ACTION).run();
        Mockito.verify(mNativeMock).handleSecondaryActionClick(nativePtr);
        messageProperties
                .get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        Mockito.verify(mNativeMock).handleDismissCallback(nativePtr, DismissReason.PRIMARY_ACTION);
    }

    /** Tests that native callbacks are not delivered if the MessageWrapper was destroyed. */
    @Test
    @SmallTest
    public void testDestroyedMessageWrapperCallbacks() {
        final long nativePtr = 1;
        MessageWrapper message = MessageWrapper.create(nativePtr, MessageIdentifier.TEST_MESSAGE);
        PropertyModel messageProperties = message.getMessageProperties();

        message.clearNativePtr();
        messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        Mockito.verify(mNativeMock, never()).handleActionClick(nativePtr);
        messageProperties.get(MessageBannerProperties.ON_SECONDARY_ACTION).run();
        Mockito.verify(mNativeMock, never()).handleSecondaryActionClick(nativePtr);
        messageProperties
                .get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        Mockito.verify(mNativeMock, never())
                .handleDismissCallback(Mockito.anyLong(), Mockito.anyInt());
    }

    /** Tests the secondary menu functionality including addition, selection and clearance of items. */
    @Test
    @SmallTest
    public void testSecondaryMenuUpdates() {
        final long nativePtr = 1;
        MessageWrapper message = MessageWrapper.create(1, MessageIdentifier.TEST_MESSAGE);

        message.setTitle("Title");
        message.setSecondaryIconResourceId(2);
        message.setPrimaryButtonText("Primary button");

        // Add secondary menu items.
        PropertyModel item1 = message.addSecondaryMenuItem(1, 0, "Item 1");
        message.addSecondaryMenuItemDivider();
        PropertyModel item2 = message.addSecondaryMenuItem(2, 0, "Item 2");
        MessageSecondaryMenuItems messageSecondaryMenuItems =
                message.getMessageSecondaryMenuItemsForTesting();
        Assert.assertEquals(
                "Size of secondary menu does not match.",
                3,
                messageSecondaryMenuItems.mMenuItems.size());

        // Select a secondary menu item.
        message.onItemSelected(item1);
        Mockito.verify(mNativeMock).handleSecondaryMenuItemSelected(nativePtr, 1);

        // Clear the secondary menu.
        message.clearSecondaryMenuItems();
        Assert.assertEquals(
                "Secondary menu is not cleared.", 0, messageSecondaryMenuItems.mMenuItems.size());
    }
}
