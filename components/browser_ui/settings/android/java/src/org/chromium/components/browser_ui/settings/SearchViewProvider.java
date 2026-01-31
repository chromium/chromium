// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for Settings Fragment equipped with Fragment-scope search to implement in order to
 * coordinate the visibility with the global settings search UI.
 */
@NullMarked
public interface SearchViewProvider {

    /** Notifies when Fragment-scope search UI visibility is updated. */
    interface Observer {
        void onUpdated(boolean visible);
    }

    /** Sets the {@link Observer}. */
    void setSearchViewObserver(Observer observer);
}
