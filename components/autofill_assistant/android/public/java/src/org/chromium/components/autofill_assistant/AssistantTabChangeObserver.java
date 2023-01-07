// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

/**
 * Observer for different tab events.
 */
public interface AssistantTabChangeObserver extends AssistantTabObserver {
    void onObservingDifferentTab(
            boolean isTabNull, @Nullable WebContents webContents, boolean isHint);
}
