// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;

/**
 * Utility class for launching the settings activity. Implementations might differ depending on
 * where Autofill Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantSettingsUtil {
    /** Launches the settings activity. */
    void launch(Context context);
}
