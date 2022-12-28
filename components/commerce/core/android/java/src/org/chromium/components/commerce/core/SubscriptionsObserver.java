// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import java.util.List;

/** An observer to notify that a (un)subscribe request has finished. */
public interface SubscriptionsObserver {
    /**
     * Invoked when a subscribe request has finished.
     *
     * @param subscriptions The list of subscriptions being added.
     * @param succeeded Whether the subscriptions are successfully added.
     */
    void onSubscribe(List<CommerceSubscription> subscriptions, boolean succeeded);

    /**
     * Invoked when an unsubscribe request has finished.
     *
     * @param subscriptions The list of subscriptions being removed.
     * @param succeeded Whether the subscriptions are successfully removed.
     */
    void onUnsubscribe(List<CommerceSubscription> subscriptions, boolean succeeded);
}
