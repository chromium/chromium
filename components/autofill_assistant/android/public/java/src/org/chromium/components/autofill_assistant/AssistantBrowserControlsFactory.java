// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

/**
 * Interface for instantiating browser control objects. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
 */
public interface AssistantBrowserControlsFactory {
    /**
     * Instantiates a new browser controls object.
     */
    AssistantBrowserControls createBrowserControls();
}
