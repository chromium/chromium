// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link MessageContainer}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MessageContainerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private MessageContainer.MessageContainerA11yDelegate mA11yDelegate;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                });
    }

    @Test
    @SmallTest
    public void testA11yDelegate() {
        MessageContainer container = new MessageContainer(sActivity, null);
        container.setA11yDelegate(mA11yDelegate);
        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(container);
        AccessibilityEvent focus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        delegate.onInitializeAccessibilityEvent(container, focus);
        verify(mA11yDelegate).onA11yFocused();
        AccessibilityEvent unfocus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
        delegate.onInitializeAccessibilityEvent(container, unfocus);
        verify(mA11yDelegate).onA11yFocusCleared();

        View child = new View(sActivity);
        container.addMessage(child);
        delegate.onRequestSendAccessibilityEvent(container, child, focus);
        verify(mA11yDelegate, times(2)).onA11yFocused();
        unfocus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
        delegate.onRequestSendAccessibilityEvent(container, child, unfocus);
        verify(mA11yDelegate, times(2)).onA11yFocusCleared();
    }

    @Test
    @SmallTest
    public void testCustomA11yActions() {
        MessageContainer container = new MessageContainer(sActivity, null);
        container.setA11yDelegate(mA11yDelegate);
        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(container);
        AccessibilityEvent focus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        delegate.onInitializeAccessibilityEvent(container, focus);

        View child = new View(sActivity);
        container.addMessage(child);
        int action = container.getA11yDismissActionIdForTesting();
        Assert.assertNotEquals("a11y action is not initialized", View.NO_ID, action);
        View child2 = new View(sActivity);
        container.addMessage(child2);

        action = container.getA11yDismissActionIdForTesting();
        ViewCompat.performAccessibilityAction(container, action, null);
        verify(mA11yDelegate, times(1)).onA11yDismiss();

        // Simulate removing child.
        container.removeMessage(child2);
        action = container.getA11yDismissActionIdForTesting();
        ViewCompat.performAccessibilityAction(container, action, null);
        verify(mA11yDelegate, times(2)).onA11yDismiss();
    }
}
