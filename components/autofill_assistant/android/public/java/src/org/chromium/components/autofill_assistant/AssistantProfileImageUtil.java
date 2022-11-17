// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.graphics.drawable.Drawable;

/**
 * Utility class for showing info pages to the user. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
 */
public interface AssistantProfileImageUtil {
    /**
     * Observer to get notifications about profile image changes.
     */
    public interface Observer {
        /**
         * Notifies that an the profile image has changed.
         * @param profileImage The profile image.
         */
        void onProfileImageChanged(Drawable profileImage);
    }

    /**
     * @param observer Observer that should be notified when new profile image changes.
     */
    public void addObserver(Observer observer);

    /**
     * @param observer Observer that was added by {@link #addObserver} and should be removed.
     */
    public void removeObserver(Observer observer);
}
