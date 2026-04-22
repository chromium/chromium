// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the AtMemoryBottomSheet. */
@NullMarked
public class AtMemoryBottomSheetCoordinator {
    private final AtMemoryBottomSheetMediator mMediator = new AtMemoryBottomSheetMediator();
    private @Nullable AtMemoryBottomSheetView mView;

    /** Delegate to receive events from the bottom sheet. */
    public interface Delegate {
        void onDismissed();
    }

    public void initialize(
            Context context, BottomSheetController sheetController, Delegate delegate) {
        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, false)
                        .build();

        mView = new AtMemoryBottomSheetView(context);
        AtMemoryBottomSheetContent content = new AtMemoryBottomSheetContent(mView.getContentView());

        mMediator.initialize(delegate, model, sheetController, content);

        PropertyModelChangeProcessor.create(model, mView, AtMemoryBottomSheetViewBinder::bind);
    }

    public void show() {
        mMediator.show();
    }

    public void destroy() {
        mMediator.destroy();
    }
}
