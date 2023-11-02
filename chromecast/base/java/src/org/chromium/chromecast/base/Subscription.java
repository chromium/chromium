// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * When closed, unsubscribe an Observer from an Observable.
 *
 * This is returned from Observable#subscribe() when subscribing an Observer to an Observable. When
 * The close() method is called on a Subscription, all open Scopes from the Observer will be closed
 * and the Observer will not be opened again.
 *
 * This is an alias for Scope, but is easier to understand in terms of its role when referred to as
 * a "Subscription".
 */
public interface Subscription extends Scope {}
