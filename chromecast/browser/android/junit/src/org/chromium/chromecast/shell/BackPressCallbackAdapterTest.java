// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chromecast.base.Box;
import org.chromium.chromecast.base.Scope;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackPressCallbackAdapterTest {
    private OnBackPressedDispatcher mDispatcher;
    private PlaceholderLifecycleOwner mLifecycleOwner;
    private BackPressCallbackAdapter mAdapter;

    private static class PlaceholderLifecycleOwner implements LifecycleOwner {
        private final LifecycleRegistry mRegistry = new LifecycleRegistry(this);

        public PlaceholderLifecycleOwner() {
            mRegistry.setCurrentState(Lifecycle.State.RESUMED);
        }

        @Override
        public Lifecycle getLifecycle() {
            return mRegistry;
        }
    }

    @Before
    public void setUp() {
        mDispatcher = new OnBackPressedDispatcher();
        mLifecycleOwner = new PlaceholderLifecycleOwner();
        mAdapter = BackPressCallbackAdapter.create(mLifecycleOwner, mDispatcher);
    }

    @Test
    public void testNoObserversLeavesCallbackDisabled() {
        assertFalse(mDispatcher.hasEnabledCallbacks());
    }

    @Test
    public void testSubscribeEnablesCallback() {
        Scope sub = mAdapter.observeBackPressedEvents().subscribe(x -> Scope.NO_OP);
        assertTrue(mDispatcher.hasEnabledCallbacks());
        sub.close();
        assertFalse(mDispatcher.hasEnabledCallbacks());
    }

    @Test
    public void testMultipleObserversSharesCallback() {
        Scope sub1 = mAdapter.observeBackPressedEvents().subscribe(x -> Scope.NO_OP);
        Scope sub2 = mAdapter.observeBackPressedEvents().subscribe(x -> Scope.NO_OP);
        assertTrue(mDispatcher.hasEnabledCallbacks());

        sub1.close();
        // Callback should still be enabled because sub2 is active.
        assertTrue(mDispatcher.hasEnabledCallbacks());

        sub2.close();
        // Now it should be disabled.
        assertFalse(mDispatcher.hasEnabledCallbacks());
    }

    @Test
    public void testDispatchesEventsToObservers() {
        Box<Integer> eventCount = new Box<>(0);
        Scope sub =
                mAdapter.observeBackPressedEvents()
                        .subscribe(
                                x -> {
                                    eventCount.value++;
                                    return Scope.NO_OP;
                                });

        mDispatcher.onBackPressed();
        assertEquals(1, (int) eventCount.value);

        mDispatcher.onBackPressed();
        assertEquals(2, (int) eventCount.value);

        sub.close();
        mDispatcher.onBackPressed();
        assertEquals(2, (int) eventCount.value);
    }

    @Test
    public void testFallbackToDefaultBackPressHandlerDelegatesToNextCallback() {
        Box<Boolean> fallbackTriggered = new Box<>(false);
        mDispatcher.addCallback(
                mLifecycleOwner,
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {
                        fallbackTriggered.value = true;
                    }
                });

        // Our adapter is added AFTER the fallback callback, so it has higher priority.
        BackPressCallbackAdapter adapter =
                BackPressCallbackAdapter.create(mLifecycleOwner, mDispatcher);
        adapter.observeBackPressedEvents()
                .subscribe(
                        x -> {
                            adapter.fallbackToDefaultBackPressHandler();
                            return Scope.NO_OP;
                        });

        mDispatcher.onBackPressed();
        assertTrue(fallbackTriggered.value);
    }
}
