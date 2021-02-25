// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for {@link ScopeChangeController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ScopeChangeControllerTest {
    @Mock
    private WebContents mWebContents;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    /**
     * Ensure that message key and web contents are registered and that all maps are cleared
     * when ref count decrements to zero.
     */
    @Test
    @SmallTest
    public void testEnqueueMultipleMessageOnSameWebContents() {
        MessageQueueManager queueManager = new MessageQueueManager();
        ScopeChangeController controller = new ScopeChangeController(queueManager);
        Object key1 = new Object();
        Object key2 = new Object();
        doReturn(Visibility.VISIBLE).when(mWebContents).getVisibility();
        doNothing().when(mWebContents).addObserver(any());
        controller.observe(key1, mWebContents);
        controller.observe(key2, mWebContents);

        Assert.assertEquals("Each message key should be registered in the map.", 2,
                controller.getMessageToWebContentsMap().size());
        Assert.assertEquals("Each web contents should be registered in the map.", 1,
                controller.getRefCountedWebContentsObserverMap().size());

        controller.stopObservation(key1);
        Assert.assertEquals("The message key should be removed in the map if it is not observed.",
                1, controller.getMessageToWebContentsMap().size());
        Assert.assertEquals("The web contents should stay in the map if it still being observed.",
                1, controller.getRefCountedWebContentsObserverMap().size());

        controller.stopObservation(key2);
        Assert.assertTrue("All maps should be cleared if ref count decrements to zero.",
                controller.getMessageToWebContentsMap().isEmpty());
        Assert.assertTrue("All maps should be cleared if ref count decrements to zero.",
                controller.getRefCountedWebContentsObserverMap().isEmpty());
    }

    /**
     * Test {@link ScopeChangeController#stopAllObservation()}.
     */
    @Test
    @SmallTest
    public void testStopAllObservation() {
        MessageQueueManager queueManager = new MessageQueueManager();
        ScopeChangeController controller = new ScopeChangeController(queueManager);
        Object key1 = new Object();
        Object key2 = new Object();
        doReturn(Visibility.VISIBLE).when(mWebContents).getVisibility();
        doNothing().when(mWebContents).addObserver(any());
        controller.observe(key1, mWebContents);
        controller.observe(key2, mWebContents);

        Assert.assertEquals("Each message key should be registered in the map.", 2,
                controller.getMessageToWebContentsMap().size());
        Assert.assertEquals("Each web contents should be registered in the map.", 1,
                controller.getRefCountedWebContentsObserverMap().size());

        controller.stopAllObservation();
        Assert.assertTrue("All maps should be cleared if ref count decrements to zero.",
                controller.getMessageToWebContentsMap().isEmpty());
        Assert.assertTrue("All maps should be cleared if ref count decrements to zero.",
                controller.getRefCountedWebContentsObserverMap().isEmpty());
    }
}
