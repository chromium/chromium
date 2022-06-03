// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressProperties.CompletionState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for the load progress bar. Adjusts view properties on ToolbarProgressBar in response
 * to changes in the associated property model.
 */
public class LoadProgressViewBinder {
    public void bind(PropertyModel model, ToolbarProgressBar view, PropertyKey propertyKey) {
        if (propertyKey == LoadProgressProperties.COMPLETION_STATE) {
            @CompletionState
            int completionState = model.get(LoadProgressProperties.COMPLETION_STATE);
            boolean done = !(completionState == CompletionState.UNFINISHED);
            if (done) {
                view.finish(completionState == CompletionState.FINISHED_DO_ANIMATE);
            } else {
                view.start();
            }
        } else if (propertyKey == LoadProgressProperties.PROGRESS) {
            float progress = model.get(LoadProgressProperties.PROGRESS);
            view.setProgress(progress);
        }
    }
}
