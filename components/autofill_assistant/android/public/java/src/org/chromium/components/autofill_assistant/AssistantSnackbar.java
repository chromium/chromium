// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

/**
 * Interface for controlling a snackbar.
 */
public interface AssistantSnackbar {
    /** Show the snackbar to the user. */
    void show();
    /** Dismiss the snackbar. */
    void dismiss();
}
