// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the business logic for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetMediator {
    private final PropertyModel mModel;
    private final AtMemoryBottomSheetCoordinator.Delegate mDelegate;

    AtMemoryBottomSheetMediator(
            AtMemoryBottomSheetCoordinator.Delegate delegate, PropertyModel model) {
        mModel = model;
        mDelegate = delegate;
    }

    void onDismissed() {
        mModel.set(AtMemoryBottomSheetProperties.VISIBLE, false);
        mDelegate.onDismissed();
    }
}
