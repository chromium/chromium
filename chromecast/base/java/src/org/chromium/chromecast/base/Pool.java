// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * An Observable with at any number of activations at a time, with mutators that add or remove
 * activation data.
 *
 * <p>Unlike a Controller, a Pool can have any number of activations at a time. When an observer is
 * subscribed, it will be notified of all current activations, and the Scopes returned by the
 * observer will only be closed when the data is removed, even if more data is added earlier.
 *
 * <p>Mutator methods on this class are sequenced: if calling one causes an Observer to call another
 * sequenced mutator method synchronously, the nested call will be deferred until the outer call is
 * finished. This ensures that all subscribed Observers are notified of all state changes.
 *
 * @param <T> The type of the activation data.
 */
public class Pool<T> implements Observable<T> {
    private static class InsertionId {}

    private final Sequencer mSequencer = new Sequencer();
    private final List<Observer<? super T>> mObservers = new ArrayList<>();
    private final Map<Both<Observer<? super T>, InsertionId>, Scope> mScopeMap = new HashMap<>();
    private final Map<InsertionId, T> mData = new HashMap<>();
    private final List<InsertionId> mInsertionOrder = new ArrayList<>();

    /**
     * Tracks this Pool with the given observer.
     *
     * <p>An Observer of a Pool may be activated with multiple items at a time. Items may be freely
     * added or removed from the Pool in any order, and duplicate items are allowed.
     *
     * <p>It is possible for multiple identical items to be added to the Pool, so make sure that if
     * you are keeping track of activations in some data structure like a Set or a Map, you are
     * taking this possibility into account. For example, if you are adding activation data to a
     * HashSet in an Observer's open() method and removing it in the respective Scope's close()
     * method, you may end up with double-removes and/or idempotent adds if the Pool contains
     * duplicate items.
     *
     * <p>When the returned Scope (referred to as a "subscription") is closed, the observer's scopes
     * will be closed in the reverse order that they were opened, and the observer will no longer be
     * notified of updates.
     */
    @Override
    public Scope subscribe(Observer<? super T> observer) {
        mSequencer.sequence(
                () -> {
                    mObservers.add(observer);
                    for (InsertionId id : mInsertionOrder) {
                        notifyEnter(observer, id);
                    }
                });
        return () ->
                mSequencer.sequence(
                        () -> {
                            for (int i = mData.size() - 1; i >= 0; i--) {
                                var id = mInsertionOrder.get(i);
                                notifyExit(observer, id);
                            }
                            mObservers.remove(observer);
                        });
    }

    /**
     * Adds data to the Pool, and returns a Scope that removes the data when closed.
     *
     * <p>You can add the same data (by equals() or identity) multiple times. The returned Scope
     * will remove the exact occurrence of the data that was added in its originating call to add().
     */
    public Scope add(@NonNull T data) {
        var id = new InsertionId();
        mSequencer.sequence(
                () -> {
                    mInsertionOrder.add(id);
                    mData.put(id, data);
                    for (Observer<? super T> observer : mObservers) {
                        notifyEnter(observer, id);
                    }
                });
        return () ->
                mSequencer.sequence(
                        () -> {
                            for (int i = mObservers.size() - 1; i >= 0; i--) {
                                notifyExit(mObservers.get(i), id);
                            }
                            mData.remove(id);
                            mInsertionOrder.remove(id);
                        });
    }

    @SuppressWarnings("Assertion")
    private void notifyEnter(Observer<? super T> observer, InsertionId id) {
        assert mSequencer.inSequence();
        T data = mData.get(id);
        Scope scope = observer.open(data);
        assert scope != null;
        mScopeMap.put(Both.of(observer, id), scope);
    }

    @SuppressWarnings("Assertion")
    private void notifyExit(Observer<? super T> observer, InsertionId id) {
        assert mSequencer.inSequence();
        try (Scope scope = mScopeMap.remove(Both.of(observer, id))) {
            assert scope != null;
        }
    }
}
