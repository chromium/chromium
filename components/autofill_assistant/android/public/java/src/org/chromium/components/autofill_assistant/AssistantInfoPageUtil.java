// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;

/**
 * Utility class for showing info pages to the user. Implementations might differ depending on where
 * Autofill Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantInfoPageUtil {
    /**
     * Shows a web page to the user. Might have limited functionality.
     */
    void showInfoPage(Context context, String url);
}
