// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import org.chromium.content_public.browser.WebContents;

/**
 * Utility class for showing feedback forms. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
 */
public interface AssistantFeedbackUtil {
    /**
     * Shows a feedback form to the user.
     */
    void showFeedback(Activity activity, WebContents webContents,
            /* @ScreenshotMode */ int screenshotMode, String debugContext);
}
