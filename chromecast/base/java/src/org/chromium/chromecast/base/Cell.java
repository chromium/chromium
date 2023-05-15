// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

/**
 * An Observable with exactly one activation at a time, which provides a way to mutate the
 * activation data.
 *
 * Mutator methods on this class are sequenced: if calling one causes an Observer to call another
 * sequenced mutator method synchronously, the nested call will be deferred until the outer call is
 * finished. This ensures that all subscribed Observers are notified of all state changes.
 *
 * @param <T> The type of the activation data.
 */
public class Cell<T> implements Observable<T> {
    private final Sequencer mSequencer = new Sequencer();
    private final List<Observer<? super T>> mObservers = new ArrayList<>();
    private final Map<Observer<? super T>, Scope> mScopeMap = new HashMap<>();
    private T mData;

    public Cell(T start) {
        assert start != null;
        mData = start;
    }

    @Override
    public Scope subscribe(Observer<? super T> observer) {
        mSequencer.sequence(() -> {
            mObservers.add(observer);
            notifyEnter(observer);
        });
        return () -> mSequencer.sequence(() -> {
            notifyExit(observer);
            mObservers.remove(observer);
        });
    }

    /**
     * Mutates the current state of the activation data. The |mutator| is called with the current
     * value of the activation data as input, and the output of that call becomes the new data.
     *
     * Observers are notified of both the deactivation of the current value and the activation of
     * the new value.
     *
     * If the new value is equivalent to the old value (according to .equals()), then observers are
     * not notified of any changes.
     */
    public void mutate(Function<? super T, ? extends T> mutator) {
        mSequencer.sequence(() -> {
            T data = mutator.apply(mData);
            if (data.equals(mData)) return;
            for (int i = mObservers.size() - 1; i >= 0; i--) {
                notifyExit(mObservers.get(i));
            }
            mData = data;
            for (int i = 0; i < mObservers.size(); i++) {
                notifyEnter(mObservers.get(i));
            }
        });
    }

    /**
     * Sets the activation data for this Cell to the given |data|, notifying observers.
     */
    public void set(T data) {
        mutate(x -> data);
    }

    private void notifyEnter(Observer<? super T> observer) {
        assert mSequencer.inSequence();
        Scope scope = observer.open(mData);
        assert scope != null;
        mScopeMap.put(observer, scope);
    }

    private void notifyExit(Observer<? super T> observer) {
        assert mSequencer.inSequence();
        Scope scope = mScopeMap.get(observer);
        assert scope != null;
        mScopeMap.remove(observer);
        scope.close();
    }
}
