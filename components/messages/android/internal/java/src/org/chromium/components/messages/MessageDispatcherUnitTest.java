// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.robolectric.annotation.LooperMode.Mode.PAUSED;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MessageDispatcherImpl}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(PAUSED)
@Features.EnableFeatures({
    MessageFeatureList.MESSAGES_FOR_ANDROID_FULLY_VISIBLE_CALLBACK,
    MessageFeatureList.MESSAGES_ANDROID_EXTRA_HISTOGRAMS
})
public class MessageDispatcherUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MessageQueueManager mQueueManager;
    @Mock private MessageAnimationCoordinator mAnimationCoordinator;

    @Test
    public void testEnqueueWindowScopedMessage() {
        doReturn(mAnimationCoordinator).when(mQueueManager).getAnimationCoordinator();
        MessageDispatcherImpl dispatcher =
                new MessageDispatcherImpl(
                        null, () -> 1, () -> 1, (x, v) -> 1L, null, mQueueManager);
        dispatcher.enqueueWindowScopedMessage(getModel(), false);
        ArgumentCaptor<ScopeKey> captor = ArgumentCaptor.forClass(ScopeKey.class);
        verify(mQueueManager).enqueueMessage(any(), any(), captor.capture(), anyBoolean());
        Assert.assertEquals(
                "The message should be of window scope if it is enqueued by"
                        + " #enqueueWindowScopedMessage.",
                MessageScopeType.WINDOW,
                captor.getValue().scopeType);
    }

    private PropertyModel getModel() {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.MESSAGE_IDENTIFIER, MessageIdentifier.TEST_MESSAGE)
                .with(MessageBannerProperties.TITLE, "test")
                .with(MessageBannerProperties.DESCRIPTION, "Description")
                .with(MessageBannerProperties.ICON, null)
                .with(
                        MessageBannerProperties.ON_PRIMARY_ACTION,
                        () -> PrimaryActionClickBehavior.DISMISS_IMMEDIATELY)
                .with(MessageBannerProperties.ON_DISMISSED, (dismissReason) -> {})
                .build();
    }
}
