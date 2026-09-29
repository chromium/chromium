// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the download toolbar button. */
@NullMarked
final class DownloadButtonViewBinder {
    public static void bind(PropertyModel model, DownloadButtonView view, PropertyKey propertyKey) {
        if (DownloadButtonProperties.IS_VISIBLE.equals(propertyKey)) {
            view.setVisibility(
                    model.get(DownloadButtonProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (DownloadButtonProperties.ON_CLICK.equals(propertyKey)) {
            view.getButton().setOnClickListener(model.get(DownloadButtonProperties.ON_CLICK));
        } else if (DownloadButtonProperties.TINT.equals(propertyKey)) {
            view.setTint(model.get(DownloadButtonProperties.TINT));
        } else if (DownloadButtonProperties.IS_INCOGNITO.equals(propertyKey)) {
            view.setIsIncognito(model.get(DownloadButtonProperties.IS_INCOGNITO));
        }
    }
}
