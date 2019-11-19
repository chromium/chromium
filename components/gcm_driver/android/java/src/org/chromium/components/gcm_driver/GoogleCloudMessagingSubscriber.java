// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.os.Bundle;

import androidx.annotation.Nullable;

import java.io.IOException;

/**
 * Helper to subscribe to and unsubscribe from Google Cloud Messaging.
 */
public interface GoogleCloudMessagingSubscriber {
    /**
     * Subscribes to a source to start receiving messages from it.
     * <p>
     * This method may perform blocking I/O and should not be called on the main thread.
     *
     * @param source The source of the notifications to subscribe to.
     * @param subtype The sub-source of the notifications.
     * @param data Additional information.
     * @return The registration id.
     * @throws IOException if the request fails.
     */
    String subscribe(String source, String subtype, @Nullable Bundle data) throws IOException;

    /**
     * Unsubscribes from a source to stop receiving messages from it.
     * <p>
     * This method may perform blocking I/O and should not be called on the main thread.
     *
     * @param source The source to unsubscribe from.
     * @param subtype The sub-source of the notifications.
     * @param data Additional information.
     * @throws IOException if the request fails.
     */
    void unsubscribe(String source, String subtype, @Nullable Bundle data) throws IOException;
}
