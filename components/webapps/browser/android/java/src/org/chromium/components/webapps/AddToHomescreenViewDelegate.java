// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

/**
 * Used by {@link AddToHomescreenDialogView} to propagate view events to {@link
 * AddToHomescreenMediator}.
 */
public interface AddToHomescreenViewDelegate {
    /** Called when the user accepts adding the item to home screen with the provided title. */
    void onAddToHomescreen(String title, @AppType int selectedType);

    /**
     * Called when the user requests app details.
     *
     * @return Whether the view should be dismissed.
     */
    boolean onAppDetailsRequested();

    /** Called when the user doesn't accept adding the item to home screen and the view is dismissed. */
    void onViewDismissed();
}
