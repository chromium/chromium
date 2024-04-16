// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Rect;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.UnguessableToken;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerGestureListener;

/** Tests for the {@link PlayerFrameCoordinator} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerFrameCoordinatorTest {
    /** Tests the {@link PlayerFrameCoordinator#setAcceptUserInput(boolean)} method. */
    @Test
    public void testUserInputToggle() {
        PlayerFrameCoordinator rootCoordinator =
                new PlayerFrameCoordinator(
                        RuntimeEnvironment.systemContext,
                        Mockito.mock(PlayerCompositorDelegate.class),
                        UnguessableToken.createForTesting(),
                        100,
                        2000,
                        0,
                        0,
                        0f,
                        true,
                        null,
                        Mockito.mock(PlayerGestureListener.class),
                        null,
                        null,
                        null);
        PlayerFrameCoordinator childCoordinator =
                new PlayerFrameCoordinator(
                        RuntimeEnvironment.systemContext,
                        Mockito.mock(PlayerCompositorDelegate.class),
                        UnguessableToken.createForTesting(),
                        100,
                        200,
                        0,
                        0,
                        0f,
                        true,
                        null,
                        Mockito.mock(PlayerGestureListener.class),
                        null,
                        null,
                        null);
        rootCoordinator.addSubFrame(childCoordinator, new Rect(10, 20, 35, 40));

        rootCoordinator.getMediator().setLayoutDimensions(100, 200);
        childCoordinator.getMediator().setLayoutDimensions(25, 20);

        // Main frame and the sub-frame should be able to scroll and scale.
        Assert.assertTrue(
                "Should be able to scroll.",
                rootCoordinator.getScrollControllerForTest().scrollBy(10f, 10f));
        Assert.assertTrue(
                "Should be able to scale.",
                rootCoordinator.getScaleControllerForTest().scaleBy(1.1f, 0f, 0f));
        Assert.assertTrue(
                "Should be able to scroll.",
                childCoordinator.getScrollControllerForTest().scrollBy(1f, 1f));
        Assert.assertTrue(
                "Should be able to scale.",
                childCoordinator.getScaleControllerForTest().scaleBy(1.1f, 0f, 0f));

        // Don't accept user input. Scaling and scrolling is not permitted.
        rootCoordinator.setAcceptUserInput(false);
        Assert.assertFalse(
                "Should not be able to scroll.",
                rootCoordinator.getScrollControllerForTest().scrollBy(10f, 10f));
        Assert.assertFalse(
                "Should not be able to scale.",
                rootCoordinator.getScaleControllerForTest().scaleBy(1.1f, 0f, 0f));
        Assert.assertFalse(
                "Should not be able to scroll.",
                childCoordinator.getScrollControllerForTest().scrollBy(1f, 1f));
        Assert.assertFalse(
                "Should not be able to scale.",
                childCoordinator.getScaleControllerForTest().scaleBy(1.1f, 0f, 0f));

        // Accept user input. All scale and scroll operations should be executed.
        rootCoordinator.setAcceptUserInput(true);
        Assert.assertTrue(
                "Should be able to scroll.",
                rootCoordinator.getScrollControllerForTest().scrollBy(10f, 10f));
        Assert.assertTrue(
                "Should be able to scale.",
                rootCoordinator.getScaleControllerForTest().scaleBy(1.1f, 0f, 0f));
        Assert.assertTrue(
                "Should be able to scroll.",
                childCoordinator.getScrollControllerForTest().scrollBy(1f, 1f));
        Assert.assertTrue(
                "Should be able to scale.",
                childCoordinator.getScaleControllerForTest().scaleBy(1.1f, 0f, 0f));
    }
}
