// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNull;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.MutableObservableValue;
import org.chromium.components.signin.ObservableValue;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Robolectric tests for {@link ObservableValue}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ObservableValueTest {
    @Test
    @SmallTest
    public void testNullAllowed() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(null);
        // Using null as a value should be allowed.
        assertNull(value.get());
    }

    @Test
    @SmallTest
    public void testAddObserverNoCallsToOnValueChanged() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(0);
        ObservableValue.Observer observer = mock(ObservableValue.Observer.class);
        value.addObserver(observer);
        verifyZeroInteractions(observer);
    }

    @Test
    @SmallTest
    public void testObserverIsNotifiedOnSet() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(0);
        ObservableValue.Observer observer = mock(ObservableValue.Observer.class);
        value.addObserver(observer);

        value.set(1);
        assertEquals(1, (int) value.get());
        verify(observer, times(1)).onValueChanged();
    }

    @Test
    @SmallTest
    public void testObserverIsNotNotifiedOnSetWithTheSameValue() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(123);
        ObservableValue.Observer observer = mock(ObservableValue.Observer.class);
        value.addObserver(observer);

        // Manually allocate the new value to make sure it's a different object and not a reference
        // to the same object from Java integer pool.
        @SuppressWarnings("UnnecessaryBoxing")
        Integer newValue = new Integer(123);

        value.set(newValue);
        assertEquals(123, (int) value.get());
        verifyZeroInteractions(observer);
    }

    @Test
    @SmallTest
    public void testObserverIsNotNotifiedAfterRemoval() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(0);
        ObservableValue.Observer observer = mock(ObservableValue.Observer.class);
        value.addObserver(observer);

        value.removeObserver(observer);
        value.set(321);
        assertEquals(321, (int) value.get());
        verifyZeroInteractions(observer);
    }

    @Test
    @SmallTest
    public void testGetReturnsUpdatedValueFromObserver() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(0);
        AtomicInteger valueHolder = new AtomicInteger(0);
        value.addObserver(() -> valueHolder.set(value.get()));

        value.set(123);
        assertEquals(123, valueHolder.get());
    }

    @Test
    @SmallTest
    public void testCanModifyObserverListFromOnValueChanged() {
        MutableObservableValue<Integer> value = new MutableObservableValue<>(0);
        AtomicInteger callCounter = new AtomicInteger(0);
        ObservableValue.Observer observer = new ObservableValue.Observer() {
            @Override
            public void onValueChanged() {
                callCounter.incrementAndGet();
                value.removeObserver(this);
            }
        };
        value.addObserver(observer);
        value.set(234);
        assertEquals("Observer should be invoked once", 1, callCounter.get());
        value.set(345);
        assertEquals("Observer should've been removed after the first call", 1, callCounter.get());
    }
}
