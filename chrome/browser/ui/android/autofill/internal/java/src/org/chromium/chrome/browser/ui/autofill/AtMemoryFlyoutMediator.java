// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the business logic for the AtMemory Flyout. */
@NullMarked
class AtMemoryFlyoutMediator {
    @SuppressWarnings("unused") // TODO(crbug.com/505255929): Remove after adding logic.
    private final PropertyModel mModel;
    private final AtMemoryFlyoutCoordinator.Delegate mDelegate;

    AtMemoryFlyoutMediator(
            AtMemoryFlyoutCoordinator.Delegate delegate,
            PropertyModel model) {
        mDelegate = delegate;
        mModel = model;
    }

    void onDismissed() {
        mDelegate.onDismissed();
    }
}
