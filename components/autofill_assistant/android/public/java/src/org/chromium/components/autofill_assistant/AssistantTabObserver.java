// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Observer for different tab events.
 */
public interface AssistantTabObserver {
    default void onContentChanged(@Nullable WebContents webContents) {}

    default void onWebContentsSwapped(
            @Nullable WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {}

    default void onDestroyed(@Nullable WebContents webContents) {}

    default void onActivityAttachmentChanged(
            @Nullable WebContents webContents, @Nullable WindowAndroid window) {}

    default void onInteractabilityChanged(
            @Nullable WebContents webContents, boolean isInteractable) {}
}
