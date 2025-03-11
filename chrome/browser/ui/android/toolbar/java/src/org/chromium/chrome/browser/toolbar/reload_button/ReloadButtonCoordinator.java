// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.widget.ImageButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Root component for the reload button. Exposes public API to change button's state and allows
 * consumers to react to button state changes.
 */
@NullMarked
public class ReloadButtonCoordinator {
    private final ReloadButtonMediator mMediator;

    public ReloadButtonCoordinator(ImageButton view) {
        final var model = new PropertyModel.Builder().build();
        mMediator = new ReloadButtonMediator(model);
        PropertyModelChangeProcessor.create(model, view, ReloadButtonViewBinder::bind);
    }

    public void destroy() {
        mMediator.destroy();
    }
}
