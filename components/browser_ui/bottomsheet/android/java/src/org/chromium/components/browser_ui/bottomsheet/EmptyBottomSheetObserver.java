// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** An empty base implementation of the {@link BottomSheetObserver} interface. */
public class EmptyBottomSheetObserver implements BottomSheetObserver {
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {}

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    @Override
    public void onSheetStateChanged(int newState, int reason) {}

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}
}
