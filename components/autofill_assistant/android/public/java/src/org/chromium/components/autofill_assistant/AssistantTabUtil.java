// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

/**
 * Utility class for closing custom tabs. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
 */
public interface AssistantTabUtil {
    /**
     * Finishes the activity if it is a CustomTabActivity.
     */
    void scheduleCloseCustomTab(Activity activity);
}
