// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import org.chromium.build.annotations.NullMarked;

/**
 * A generic interface for components that manage a collection of {@link BackPressHandler}s.
 *
 * <p>This interface provides a stable, low-level abstraction that UI components can depend on
 * without needing to know about the concrete implementation of the manager (e.g., {@code
 * BackPressManager}). This is crucial for avoiding circular dependencies between low-level UI
 * modules (like `native_page`) and higher-level browser modules.
 *
 * <p>By depending on this interface, a component like {@code BasicNativePage} can centralize the
 * logic for registering and unregistering a back press handler during its view's lifecycle, while
 * the concrete `BackPressManager` can live in a higher-level module and implement this interface.
 */
@NullMarked
public interface BackPressHandlerRegistry {
    /**
     * Adds a {@link BackPressHandler} to the registry. The handler will be prioritized based on its
     * type.
     *
     * @param handler The handler to be added.
     * @param type The {@link BackPressHandler.Type} of the handler, used to determine its priority.
     */
    void addHandler(BackPressHandler handler, @BackPressHandler.Type int type);

    /**
     * Removes a {@link BackPressHandler} from the registry.
     *
     * @param handler The handler to be removed.
     */
    void removeHandler(BackPressHandler handler);
}
