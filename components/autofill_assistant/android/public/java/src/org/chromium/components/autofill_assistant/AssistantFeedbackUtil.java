// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import org.chromium.content_public.browser.WebContents;

/**
 * Utility class for showing feedback forms. Implementations might differ depending on where
 * Autofill Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantFeedbackUtil {
    /**
     * Shows a feedback form to the user.
     */
    void showFeedback(Activity activity, WebContents webContents,
            /* @ScreenshotMode */ int screenshotMode, String debugContext);
}
