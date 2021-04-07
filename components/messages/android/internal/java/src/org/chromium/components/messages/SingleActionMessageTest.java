// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Tests for {@link SingleActionMessage}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SingleActionMessageTest {
    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();
    @ClassRule
    public static BaseActivityTestRule<DummyUiActivity> sActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    private static Activity sActivity;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private Callback<Animator> mAnimatorStartCallback;

    private CallbackHelper mDismissCallback;
    private SingleActionMessage.DismissCallback mEmptyDismissCallback =
            (model, dismissReason) -> {};

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sActivity = sActivityTestRule.getActivity(); });
    }

    @Before
    public void setupTest() throws Exception {
        mDismissCallback = new CallbackHelper();
    }

    @Test
    @MediumTest
    public void testAddAndRemoveSingleActionMessage() throws Exception {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        SingleActionMessage message = new SingleActionMessage(
                container, model, mEmptyDismissCallback, () -> 0, () -> 0L, mAnimatorStartCallback);
        final MessageBannerCoordinator messageBanner = Mockito.mock(MessageBannerCoordinator.class);
        final MessageBannerView view = new MessageBannerView(sActivity, null);
        view.setId(R.id.message_banner);
        message.setMessageBannerForTesting(messageBanner);
        message.setViewForTesting(view);
        message.show();
        Assert.assertEquals(
                "Message container should have one message view after the message is shown.", 1,
                container.getChildCount());
        message.hide(true, () -> {});
        // Let's pretend the animation ended, and the mediator called the callback as a result.
        final ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(messageBanner).hide(anyBoolean(), runnableCaptor.capture());
        runnableCaptor.getValue().run();
        Assert.assertEquals(
                "Message container should not have any view after the message is hidden.", 0,
                container.getChildCount());
        message.dismiss(DismissReason.UNKNOWN);
        mDismissCallback.waitForFirst(
                "Dismiss callback should be called when message is dismissed");
    }

    @Test
    @MediumTest
    public void testAutoDismissDuration() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        long duration = 42;
        SingleActionMessage message = new SingleActionMessage(container, model,
                mEmptyDismissCallback, () -> 0, () -> duration, mAnimatorStartCallback);
        Assert.assertEquals("Autodismiss duration is not propagated correctly.", duration,
                message.getAutoDismissDuration());
    }

    @Test(expected = IllegalStateException.class)
    @MediumTest
    public void testAddMultipleSingleActionMessage() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel();
        PropertyModel m2 = createBasicSingleActionMessageModel();
        SingleActionMessage message1 = new SingleActionMessage(
                container, m1, mEmptyDismissCallback, () -> 0, () -> 0L, mAnimatorStartCallback);
        final MessageBannerCoordinator messageBanner1 =
                Mockito.mock(MessageBannerCoordinator.class);
        final MessageBannerView view1 = new MessageBannerView(sActivity, null);
        view1.setId(R.id.message_banner);
        message1.setMessageBannerForTesting(messageBanner1);
        message1.setViewForTesting(view1);
        SingleActionMessage message2 = new SingleActionMessage(
                container, m2, mEmptyDismissCallback, () -> 0, () -> 0L, mAnimatorStartCallback);
        final MessageBannerCoordinator messageBanner2 =
                Mockito.mock(MessageBannerCoordinator.class);
        final MessageBannerView view2 = new MessageBannerView(sActivity, null);
        view2.setId(R.id.message_banner);
        message2.setMessageBannerForTesting(messageBanner2);
        message2.setViewForTesting(view2);
        message1.show();
        message2.show();
    }

    private PropertyModel createBasicSingleActionMessageModel() {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.TITLE, "test")
                .with(MessageBannerProperties.DESCRIPTION, "Description")
                .with(MessageBannerProperties.ICON,
                        ApiCompatibilityUtils.getDrawable(
                                sActivity.getResources(), android.R.drawable.ic_menu_add))
                .with(MessageBannerProperties.ON_PRIMARY_ACTION, () -> {})
                .with(MessageBannerProperties.ON_TOUCH_RUNNABLE, () -> {})
                .with(MessageBannerProperties.ON_DISMISSED,
                        (dismissReason) -> { mDismissCallback.notifyCalled(); })
                .build();
    }
}
