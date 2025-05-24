// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * An Observable that can be subscribed to multiple times.
 *
 * <p>This is a marker class that allows statically checking that an Observable is shared. A shared
 * Observable can be subscribed to arbitrarily many times, but will only do "work" for the first
 * subscription. See the share() method of Observable for more details.
 *
 * @param <T> Type of the observable.
 */
public final class SharedObservable<T> implements Observable<T> {
    private final Observable<T> mSrc;
    private final Pool<T> mCache = new Pool<>();
    private final OwnedScope mLiveness = new OwnedScope();
    private int mNumObservers;

    /**
     * Creates a SharedObservable from an Observable. Returns the original Observable if it is
     * already a SharedObservable.
     *
     * <p>Prefer to use Observable.share() instead of this method.
     */
    public static <T> SharedObservable<T> from(Observable<T> mSrc) {
        if (mSrc instanceof SharedObservable) {
            return (SharedObservable<T>) mSrc;
        }
        return new SharedObservable<>(mSrc);
    }

    private SharedObservable(Observable<T> mSrc) {
        this.mSrc = mSrc;
    }

    /**
     * Does the same thing as Observable.subscribe(), but ensures that the wrapped Observable is
     * only subscribed to once.
     *
     * <p>Observers are subscribed to a mCached copy of the data from |mSrc|, which is actively
     * updated only as long as there are any observers subscribed to |this|.
     */
    @Override
    public final Scope subscribe(Observer<? super T> observer) {
        addObserver();
        return mCache.subscribe(observer).and(this::removeObserver);
    }

    /**
     * Returns this SharedObservable.
     *
     * <p>This makes share() an idempotent operation on any Observable, making it safe to call
     * share() on a given Observable that may happen to be a SharedObservable without worrying about
     * incurring extra overhead by adding more layers of caching and subscription-counting.
     */
    @Override
    public final SharedObservable<T> share() {
        return this;
    }

    private void addObserver() {
        if (mNumObservers == 0) {
            mLiveness.set(mSrc.subscribe(mCache::add));
        }
        mNumObservers++;
    }

    private void removeObserver() {
        mNumObservers--;
        if (mNumObservers == 0) {
            mLiveness.close();
        }
    }
}
