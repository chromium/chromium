// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

/**
 * Utility class for closing custom tabs. Implementations might differ depending on where
 * Autofill Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantTabUtil {
    /**
     * Finishes the activity if it is a CustomTabActivity.
     */
    void scheduleCloseCustomTab(Activity activity);
}
