// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.textbubble;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/** Back gesture handler for {@link TextBubble}. */
@NullMarked
public class TextBubbleBackPressHandler implements BackPressHandler {
    private final SettableNonNullObservableSupplier<Boolean> mSupplier =
            ObservableSuppliers.createNonNull(false);

    private final Callback<Integer> mCallback = (count) -> mSupplier.set(count != 0);

    public TextBubbleBackPressHandler() {
        TextBubble.getCountSupplier().addSyncObserverAndPostIfNonNull(mCallback);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        boolean bubbleShowing = TextBubble.getCountSupplier().get() > 0;
        TextBubble.dismissBubbles();
        return bubbleShowing ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mSupplier;
    }

    public void destroy() {
        TextBubble.getCountSupplier().removeObserver(mCallback);
    }
}
