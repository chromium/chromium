// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

/**
 * Interface for instantiating browser control objects. Implementations might differ depending on
 * where Autofill Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantBrowserControlsFactory {
    /**
     * Instantiates a new browser controls object.
     */
    AssistantBrowserControls createBrowserControls();
}
