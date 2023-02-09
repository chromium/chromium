// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;

/** Unit tests for {@link ComposedBrowserControlsVisibilityDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ComposedBrowserControlsVisibilityDelegateTest {
    private ComposedBrowserControlsVisibilityDelegate mComposedDelegate;
    private TestVisibilityDelegate mDelegate1;
    private TestVisibilityDelegate mDelegate2;
    private TestVisibilityDelegate mDelegate3;

    @Before
    public void setUp() {
        mDelegate1 = new TestVisibilityDelegate();
        mDelegate2 = new TestVisibilityDelegate();
        mDelegate3 = new TestVisibilityDelegate();
        mComposedDelegate =
                new ComposedBrowserControlsVisibilityDelegate(mDelegate1, mDelegate2, mDelegate3);
    }

    private int composedState() {
        return mComposedDelegate.get();
    }

    @Test
    public void testSimpleConstraints_Both() {
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());
    }

    @Test
    public void testSimpleConstraints_Hidden() {
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate3.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());
        mDelegate3.set(BrowserControlsState.BOTH);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate2.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());
        mDelegate2.set(BrowserControlsState.BOTH);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate1.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());
        mDelegate1.set(BrowserControlsState.BOTH);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());
    }

    @Test
    public void testSimpleConstraints_Shown() {
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate3.set(BrowserControlsState.SHOWN);
        Assert.assertEquals(BrowserControlsState.SHOWN, composedState());
        mDelegate3.set(BrowserControlsState.BOTH);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate2.set(BrowserControlsState.SHOWN);
        Assert.assertEquals(BrowserControlsState.SHOWN, composedState());
        mDelegate2.set(BrowserControlsState.BOTH);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate1.set(BrowserControlsState.SHOWN);
        Assert.assertEquals(BrowserControlsState.SHOWN, composedState());
        mDelegate1.set(BrowserControlsState.BOTH);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());
    }

    @Test
    public void testMixedConstraints_HiddenTakesPriority() {
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());

        mDelegate1.set(BrowserControlsState.BOTH);
        mDelegate2.set(BrowserControlsState.SHOWN);
        mDelegate3.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());

        mDelegate1.set(BrowserControlsState.SHOWN);
        mDelegate2.set(BrowserControlsState.SHOWN);
        mDelegate3.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());

        mDelegate1.set(BrowserControlsState.HIDDEN);
        mDelegate2.set(BrowserControlsState.SHOWN);
        mDelegate3.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());

        mDelegate1.set(BrowserControlsState.HIDDEN);
        mDelegate2.set(BrowserControlsState.BOTH);
        mDelegate3.set(BrowserControlsState.SHOWN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());
    }

    @Test
    public void testObserver() {
        Callback<Integer> callback = Mockito.mock(Callback.class);
        mComposedDelegate.addObserver(callback);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verify(callback).onResult(BrowserControlsState.BOTH);
        Mockito.reset(callback);

        mDelegate2.set(BrowserControlsState.SHOWN);
        Mockito.verify(callback).onResult(BrowserControlsState.SHOWN);

        mDelegate3.set(BrowserControlsState.HIDDEN);
        Mockito.verify(callback).onResult(BrowserControlsState.HIDDEN);

        mDelegate2.set(BrowserControlsState.HIDDEN);
        Mockito.verifyNoMoreInteractions(callback);
        mDelegate2.set(BrowserControlsState.BOTH);
        Mockito.verifyNoMoreInteractions(callback);
        mDelegate3.set(BrowserControlsState.BOTH);
        Mockito.verify(callback).onResult(BrowserControlsState.BOTH);
    }

    @Test
    public void testAddDelegate_ObservesChanges() {
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());
        TestVisibilityDelegate newDelegate = new TestVisibilityDelegate();
        mComposedDelegate.addDelegate(newDelegate);
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());
        newDelegate.set(BrowserControlsState.HIDDEN);
        Assert.assertEquals(BrowserControlsState.HIDDEN, composedState());
    }

    @Test
    public void testAddDelegate_WithExistingState() {
        Assert.assertEquals(BrowserControlsState.BOTH, composedState());
        TestVisibilityDelegate newDelegate = new TestVisibilityDelegate();
        newDelegate.set(BrowserControlsState.SHOWN);
        mComposedDelegate.addDelegate(newDelegate);
        Assert.assertEquals(BrowserControlsState.SHOWN, composedState());
    }

    private static class TestVisibilityDelegate extends BrowserControlsVisibilityDelegate {
        public TestVisibilityDelegate() {
            super(BrowserControlsState.BOTH);
        }
    }
}
