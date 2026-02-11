// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;

/**
 * A transitive implementation of {@link TopInsetProvider} that wraps a supplier for observing.
 *
 * <p>This class provides a unified solution for managing a {@link TopInsetProvider} that may not be
 * immediately available. Observers can be added before the provider is set, and they will be
 * automatically registered once the provider becomes available.
 *
 * <p>This class is not thread-safe and should only be accessed from a single thread.
 */
@NullMarked
public class TransitiveTopInsetProvider implements TopInsetProvider {
    private final SettableMonotonicObservableSupplier<TopInsetProvider> mTopInsetProviderSupplier;
    private final Callback<TopInsetProvider> mTopInsetProviderAvailableCallback;

    /**
     * List of observers waiting for the provider to become available. Once the provider is set, all
     * pending observers are registered and this list is cleared.
     */
    private final ObserverList<TopInsetProvider.Observer> mPendingObservers = new ObserverList<>();

    /**
     * Tracks whether we have registered a callback with the supplier to be notified when the
     * provider becomes available. This flag is used to avoid redundant callback registrations.
     */
    private boolean mIsListeningToSupplier;

    /** Creates a new {@link TransitiveTopInsetProvider}. */
    public TransitiveTopInsetProvider() {
        mTopInsetProviderSupplier = ObservableSuppliers.createMonotonic();
        mTopInsetProviderAvailableCallback = this::onTopInsetProviderAvailable;
    }

    /**
     * Sets the {@link TopInsetProvider}.
     *
     * <p>Once set, all pending observers will be automatically registered with the provider and
     * will start receiving inset change notifications. This method should only be called once, as
     * the underlying supplier is monotonic.
     *
     * @param topInsetProvider The provider to set. Must not be null.
     */
    public void set(TopInsetProvider topInsetProvider) {
        mTopInsetProviderSupplier.set(topInsetProvider);
    }

    /**
     * Adds an observer to be notified of top inset changes.
     *
     * <p>If the {@link TopInsetProvider} is already available, the observer is registered with it
     * immediately and will receive callbacks for any future inset changes. If the provider is not
     * yet available, the observer is queued and will be automatically registered once the provider
     * is set.
     *
     * <p>The same observer instance can be safely added multiple times, though this is generally
     * not recommended. Use {@link #removeObserver(TopInsetProvider.Observer)} to remove the
     * observer when no longer needed to avoid memory leaks.
     *
     * @param observer The observer to add. Must not be null.
     */
    @Override
    public void addObserver(TopInsetProvider.Observer observer) {
        var topInsetProvider = mTopInsetProviderSupplier.get();
        if (topInsetProvider != null) {
            // Provider is already available, add observer directly.
            topInsetProvider.addObserver(observer);
        } else {
            // Provider not yet available, add to pending list.
            mPendingObservers.addObserver(observer);

            // Register supplier callback only once when the first pending observer is added.
            if (!mIsListeningToSupplier) {
                mTopInsetProviderSupplier.addSyncObserverAndPostIfNonNull(
                        mTopInsetProviderAvailableCallback);
                mIsListeningToSupplier = true;
            }
        }
    }

    /**
     * Removes an observer from receiving top inset change notifications.
     *
     * <p>This method removes the observer from both the active provider (if available) and the
     * pending observer list. It is safe to call this method even if the observer was never added.
     *
     * <p>If this was the last pending observer, the internal supplier callback is automatically
     * unregistered to avoid unnecessary overhead.
     *
     * @param observer The observer to remove. Must not be null.
     */
    @Override
    public void removeObserver(TopInsetProvider.Observer observer) {
        var topInsetProvider = mTopInsetProviderSupplier.get();
        if (topInsetProvider != null) {
            // Remove from provider if it's available.
            topInsetProvider.removeObserver(observer);
        }

        // Always attempt to remove from pending list (no-op if not present).
        mPendingObservers.removeObserver(observer);

        // Unregister supplier callback if no more pending observers remain.
        if (mIsListeningToSupplier && mPendingObservers.isEmpty()) {
            mTopInsetProviderSupplier.removeObserver(mTopInsetProviderAvailableCallback);
            mIsListeningToSupplier = false;
        }
    }

    /**
     * Destroys the {@link TransitiveTopInsetProvider} and its underlying {@link TopInsetProvider}.
     */
    @Override
    public void destroy() {
        var topInsetProvider = mTopInsetProviderSupplier.get();
        if (topInsetProvider != null) {
            topInsetProvider.destroy();
        }
        mPendingObservers.clear();
    }

    /**
     * Callback invoked when the {@link TopInsetProvider} becomes available.
     *
     * @param topInsetProvider The newly available provider. Guaranteed to be non-null.
     */
    private void onTopInsetProviderAvailable(TopInsetProvider topInsetProvider) {
        // Register all pending observers with the provider.
        for (var observer : mPendingObservers) {
            topInsetProvider.addObserver(observer);
        }
        mPendingObservers.clear();

        // Unregister supplier callback since all pending observers have been processed.
        if (mIsListeningToSupplier) {
            mTopInsetProviderSupplier.removeObserver(mTopInsetProviderAvailableCallback);
            mIsListeningToSupplier = false;
        }
    }
}
