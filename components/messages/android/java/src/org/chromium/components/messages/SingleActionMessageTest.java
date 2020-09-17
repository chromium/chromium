// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Tests for {@link SingleActionMessage}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SingleActionMessageTest extends DummyUiActivityTestCase {
    private CallbackHelper mDismissCallback;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mDismissCallback = new CallbackHelper();
    }

    @Test
    @SmallTest
    public void testAddAndRemoveSingleActionMessage() throws Exception {
        MessageContainer container = new MessageContainer(getActivity(), null);
        PropertyModel model = createBasicSingleActionMessageModel();
        SingleActionMessage message = new SingleActionMessage(container, model);
        message.show();
        Assert.assertEquals(
                "Message container should have one message view after the message is shown.", 1,
                container.getChildCount());
        message.hide();
        Assert.assertEquals(
                "Message container should not have any view after the message is hidden.", 0,
                container.getChildCount());
        message.dismiss();
        mDismissCallback.waitForFirst(
                "Dismiss callback should be called when message is dismissed");
    }

    @Test(expected = IllegalStateException.class)
    @SmallTest
    public void testAddMultipleSingleActionMessage() {
        MessageContainer container = new MessageContainer(getActivity(), null);
        PropertyModel m1 = createBasicSingleActionMessageModel();
        PropertyModel m2 = createBasicSingleActionMessageModel();
        SingleActionMessage message1 = new SingleActionMessage(container, m1);
        SingleActionMessage message2 = new SingleActionMessage(container, m2);
        message1.show();
        message2.show();
    }

    private PropertyModel createBasicSingleActionMessageModel() {
        Activity activity = getActivity();
        return new PropertyModel.Builder(MessageBannerProperties.SINGLE_ACTION_MESSAGE_KEYS)
                .with(MessageBannerProperties.TITLE, "test")
                .with(MessageBannerProperties.DESCRIPTION, "Description")
                .with(MessageBannerProperties.ICON,
                        ApiCompatibilityUtils.getDrawable(
                                activity.getResources(), android.R.drawable.ic_menu_add))
                .with(MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER, (v) -> {})
                .with(MessageBannerProperties.ON_DISMISSED,
                        () -> { mDismissCallback.notifyCalled(); })
                .build();
    }
}
