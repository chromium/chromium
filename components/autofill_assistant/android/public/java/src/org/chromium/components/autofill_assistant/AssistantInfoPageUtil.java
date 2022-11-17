// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;

/**
 * Utility class for showing info pages to the user. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
 */
public interface AssistantInfoPageUtil {
    /**
     * Shows a web page to the user. Might have limited functionality.
     */
    void showInfoPage(Context context, String url);
}
