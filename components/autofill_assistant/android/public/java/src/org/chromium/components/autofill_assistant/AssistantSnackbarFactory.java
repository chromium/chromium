// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;
import org.chromium.base.Callback;

/**
 * Factory for creating snackbars. Implementations might differ depending on where Autofill
 * Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantSnackbarFactory {
    /**
     * Creates a snackbar. The callback is called once the snackbar is gone, after the delay has
     * passed or after the user clicked undo.
     */
    AssistantSnackbar createSnackbar(
            int delayMs, String message, String undoString, Callback<Boolean> callback);
}
