// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MessageContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageContainerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private MessageContainer.MessageContainerA11yDelegate mA11yDelegate;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testA11yDelegate() {
        MessageContainer container = new MessageContainer(mContext, null);
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

        View child = new View(mContext);
        container.addMessage(child);
        delegate.onRequestSendAccessibilityEvent(container, child, focus);
        verify(mA11yDelegate, times(2)).onA11yFocused();
        unfocus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
        delegate.onRequestSendAccessibilityEvent(container, child, unfocus);
        verify(mA11yDelegate, times(2)).onA11yFocusCleared();
    }

    @Test
    public void testA11yDelegate_multipleChildViews() {
        MessageContainer container = new MessageContainer(mContext, null);
        container.setA11yDelegate(mA11yDelegate);
        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(container);

        View child1 = new View(mContext);
        View child2 = new View(mContext);
        container.addMessage(child1);
        container.addMessage(child2);

        AccessibilityEvent focus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        AccessibilityEvent unfocus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);

        // Focus first child: should notify onA11yFocused.
        delegate.onRequestSendAccessibilityEvent(container, child1, focus);
        verify(mA11yDelegate, times(1)).onA11yFocused();

        // Focus second child while first is still focused: should not trigger extra onA11yFocused.
        delegate.onRequestSendAccessibilityEvent(container, child2, focus);
        verify(mA11yDelegate, times(1)).onA11yFocused();

        // Clear focus on first child: second is still focused, so onA11yFocusCleared should not be
        // called yet.
        delegate.onRequestSendAccessibilityEvent(container, child1, unfocus);
        verify(mA11yDelegate, times(0)).onA11yFocusCleared();

        // Clear focus on second child: all children unfocused, should notify onA11yFocusCleared.
        delegate.onRequestSendAccessibilityEvent(container, child2, unfocus);
        verify(mA11yDelegate, times(1)).onA11yFocusCleared();
    }

    @Test
    public void testCustomA11yActions() {
        MessageContainer container = new MessageContainer(mContext, null);
        container.setA11yDelegate(mA11yDelegate);
        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(container);
        AccessibilityEvent focus =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        delegate.onInitializeAccessibilityEvent(container, focus);

        View child = new View(mContext);
        container.addMessage(child);
        int action = container.getA11yDismissActionIdForTesting();
        assertNotEquals("a11y action is not initialized", View.NO_ID, action);
        View child2 = new View(mContext);
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
