// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/**
 * An interface for the MessageDispatcher owning object.
 */
public interface ManagedMessageDispatcher
        extends MessageDispatcher, MessageDispatcherProvider.Unowned {}
