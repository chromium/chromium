// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.view.View;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds properties for the AtMemoryFlyout. */
@NullMarked
class AtMemoryFlyoutViewBinder {
    private AtMemoryFlyoutViewBinder() {}

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        // TODO(crbug.com/505257277): Handle property updates when properties are added.
        assert false : "Unhandled property: " + propertyKey;
    }
}
