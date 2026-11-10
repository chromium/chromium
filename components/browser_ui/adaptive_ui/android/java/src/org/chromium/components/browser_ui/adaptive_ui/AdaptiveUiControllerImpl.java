// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Window-scoped {@link AdaptiveUiController}. Each entry passes in its allowed containers; this
 * class does not decide which containers an entry allows. On each {@link #show} it picks one of
 * them: a modal dialog on desktop form factors ({@link DeviceInfo#isDesktop()}) and a bottom sheet
 * elsewhere. The container never changes while the entry is showing.
 *
 * <p>Each {@link #show} creates one {@link Presentation}, which holds the entry's single active
 * coordinator ({@link AdaptiveUiDialogCoordinator} or {@link AdaptiveUiSheetCoordinator}). This
 * class keeps no queue: the container ({@link ModalDialogManager} or {@link BottomSheetController})
 * owns the queue, and the coordinator may wait in it. This class picks the container and turns the
 * coordinator's callbacks into {@link AdaptiveUiObserver} calls.
 */
@NullMarked
public final class AdaptiveUiControllerImpl implements AdaptiveUiController {
    /**
     * One showing of one entry, from {@link #show} until the entry is dismissed. Exactly one of the
     * coordinators is set: the one for the container chosen for this presentation.
     */
    private static final class Presentation {
        @Nullable AdaptiveUiDialogCoordinator mDialogCoordinator;
        @Nullable AdaptiveUiSheetCoordinator mSheetCoordinator;

        /** Whether the container has shown the entry (it is not just waiting in a queue). */
        boolean mShown;

        /** Whether the observer's onWillShow is still running, so no coordinator exists yet. */
        private boolean isInOnWillShow() {
            return mDialogCoordinator == null && mSheetCoordinator == null;
        }

        /** Hands the entry to its container. */
        void show() {
            if (mDialogCoordinator != null) {
                mDialogCoordinator.show();
            } else {
                assumeNonNull(mSheetCoordinator).show();
            }
        }

        /**
         * Closes the entry in its container; the coordinator reports the dismissal.
         *
         * @param reason Why the entry is closed.
         * @param animate Whether to animate, where the container supports it.
         */
        void hide(@DismissReason int reason, boolean animate) {
            if (mDialogCoordinator != null) {
                mDialogCoordinator.hide(reason);
            } else {
                assumeNonNull(mSheetCoordinator).hide(reason, animate);
            }
        }
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final BottomSheetController mBottomSheetController;
    private final Map<AdaptiveUiEntry, Presentation> mPresentations = new HashMap<>();
    private boolean mDestroyed;

    /**
     * Creates the controller for {@code windowAndroid} and attaches it, so {@link
     * AdaptiveUiController#from} returns it until {@link #destroy()}.
     *
     * @param windowAndroid The window this controller presents entries in.
     * @param modalDialogManager The window's {@link ModalDialogManager}.
     * @param bottomSheetController The window's {@link BottomSheetController}.
     */
    public AdaptiveUiControllerImpl(
            WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController) {
        Context context = windowAndroid.getContext().get();
        assert context != null : "WindowAndroid has no Context.";
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mBottomSheetController = bottomSheetController;
        AdaptiveUiControllerProvider.attach(windowAndroid, this);
    }

    /**
     * Closes every entry that is showing or queued with {@link DismissReason#TAB_CLOSED} and
     * detaches from the window.
     */
    public void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        // Close queued entries first, so closing a showing one doesn't pull a queued one on screen.
        List<Map.Entry<AdaptiveUiEntry, Presentation>> presentations =
                new ArrayList<>(mPresentations.entrySet());
        for (Map.Entry<AdaptiveUiEntry, Presentation> item : presentations) {
            if (!item.getValue().mShown) {
                close(item.getKey(), DismissReason.TAB_CLOSED, /* animate= */ false);
            }
        }
        for (Map.Entry<AdaptiveUiEntry, Presentation> item : presentations) {
            if (item.getValue().mShown) {
                close(item.getKey(), DismissReason.TAB_CLOSED, /* animate= */ false);
            }
        }
        AdaptiveUiControllerProvider.detach(this);
    }

    @Override
    public void show(AdaptiveUiEntry entry) {
        assert !mDestroyed : "A destroyed controller should not have #show invoked on it.";
        // An entry that is already showing or queued stays in its current container.
        if (mDestroyed || mPresentations.containsKey(entry)) return;

        @AdaptiveUiContainer int container = selectContainer(entry.getAllowedContainers());
        AdaptiveUiObserver observer = entry.getObserver();
        // Registered before onWillShow, so a hide() or destroy() from the observer finds the entry.
        Presentation presentation = new Presentation();
        mPresentations.put(entry, presentation);
        // Fires before the coordinator binds the entry's View into the container.
        if (observer != null) observer.onWillShow(container);
        // The observer may have hidden the entry (or destroyed this controller) in onWillShow.
        if (mPresentations.get(entry) != presentation) return;

        Runnable onShown =
                () -> {
                    if (mDestroyed) return;
                    presentation.mShown = true;
                    notifyShown(entry);
                };
        Callback<Integer> onDismissed =
                reason -> {
                    mPresentations.remove(entry);
                    notifyDismissed(entry, reason);
                };
        if (container == AdaptiveUiContainer.MODAL_DIALOG) {
            presentation.mDialogCoordinator =
                    new AdaptiveUiDialogCoordinator(
                            mContext,
                            entry.getPropertyModel(),
                            entry.getScope(),
                            mModalDialogManager,
                            onShown,
                            onDismissed);
        } else {
            presentation.mSheetCoordinator =
                    new AdaptiveUiSheetCoordinator(
                            mContext,
                            entry.getPropertyModel(),
                            entry.getScope(),
                            mBottomSheetController,
                            onShown,
                            onDismissed);
        }
        presentation.show();
    }

    @Override
    public void hide(AdaptiveUiEntry entry, @DismissReason int reason) {
        close(entry, reason, /* animate= */ true);
    }

    /**
     * Closes {@code entry} if it is showing or queued. An entry hidden from its observer's
     * onWillShow has no coordinator yet, so it is dismissed here directly.
     */
    private void close(AdaptiveUiEntry entry, @DismissReason int reason, boolean animate) {
        Presentation presentation = mPresentations.get(entry);
        if (presentation == null) return;
        if (presentation.isInOnWillShow()) {
            mPresentations.remove(entry);
            notifyDismissed(entry, reason);
            return;
        }
        presentation.hide(reason, animate);
    }

    /**
     * Picks the container for one {@link #show}: a dialog on desktop form factors and a sheet
     * elsewhere, falling back to the other one when the preferred one isn't allowed.
     */
    static @AdaptiveUiContainer int selectContainer(@AdaptiveUiContainer int allowedContainers) {
        boolean dialogAllowed = (allowedContainers & AdaptiveUiContainer.MODAL_DIALOG) != 0;
        boolean sheetAllowed = (allowedContainers & AdaptiveUiContainer.BOTTOM_SHEET) != 0;
        if (!dialogAllowed && !sheetAllowed) {
            throw new IllegalArgumentException(
                    "AdaptiveUiController needs BOTTOM_SHEET or MODAL_DIALOG in allowed"
                            + " containers.");
        }
        if (DeviceInfo.isDesktop()) {
            return dialogAllowed
                    ? AdaptiveUiContainer.MODAL_DIALOG
                    : AdaptiveUiContainer.BOTTOM_SHEET;
        }
        return sheetAllowed ? AdaptiveUiContainer.BOTTOM_SHEET : AdaptiveUiContainer.MODAL_DIALOG;
    }

    private static void notifyShown(AdaptiveUiEntry entry) {
        AdaptiveUiObserver observer = entry.getObserver();
        if (observer != null) observer.onShown();
    }

    private static void notifyDismissed(AdaptiveUiEntry entry, @DismissReason int reason) {
        AdaptiveUiObserver observer = entry.getObserver();
        if (observer != null) observer.onDismissed(reason);
    }
}
