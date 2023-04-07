// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

/** An observer to notify that a (un)subscribe request has finished. */
public interface SubscriptionsObserver {
    /**
     * Invoked when a subscribe request has finished.
     *
     * @param subscription The subscription being added.
     * @param succeeded Whether the subscription is successfully added.
     */
    void onSubscribe(CommerceSubscription subscription, boolean succeeded);

    /**
     * Invoked when an unsubscribe request has finished.
     *
     * @param subscription The subscription being removed.
     * @param succeeded Whether the subscription is successfully removed.
     */
    void onUnsubscribe(CommerceSubscription subscription, boolean succeeded);
}
