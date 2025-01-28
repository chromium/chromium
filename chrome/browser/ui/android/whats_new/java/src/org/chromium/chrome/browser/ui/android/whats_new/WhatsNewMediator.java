// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import org.chromium.chrome.browser.ui.android.whats_new.WhatsNewProperties.ViewState;
import org.chromium.ui.modelutil.PropertyModel;

public class WhatsNewMediator {
    private final PropertyModel mModel;

    WhatsNewMediator(PropertyModel model) {
        mModel = model;
    }

    void showBottomSheet() {
        setState(ViewState.OVERVIEW);
    }

    void onBackButtonPressed() {
        if (mModel.get(WhatsNewProperties.VIEW_STATE) == ViewState.DETAIL) {
            setState(ViewState.OVERVIEW);
        } else {
            setState(ViewState.HIDDEN);
        }
    }

    void setState(@ViewState int state) {
        mModel.set(WhatsNewProperties.VIEW_STATE, state);
    }
}
