// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Window-scoped {@link AdaptiveUiController}. Each entry passes in its allowed containers; this
 * class does not decide which containers an entry allows. On each {@link #show} it picks one of
 * them: a modal dialog on desktop form factors ({@link DeviceInfo#isDesktop()}) and a bottom sheet
 * elsewhere. The container never changes while the entry is showing.
 *
 * <p>Each {@link #show} creates one {@link Presenter} ({@link AdaptiveUiDialogCoordinator} or
 * {@link AdaptiveUiSheetCoordinator}) for the entry. This class keeps no queue: the container
 * ({@link ModalDialogManager} or {@link BottomSheetController}) owns the queue, and the coordinator
 * may wait in it. This class picks the container and turns the coordinator's callbacks into {@link
 * AdaptiveUiObserver} calls.
 */
@NullMarked
public final class AdaptiveUiControllerImpl implements AdaptiveUiController {
    /**
     * Shows one entry in one container (a modal dialog or a bottom sheet), from {@link #show} until
     * the entry is dismissed. Implemented by {@link AdaptiveUiDialogCoordinator} and {@link
     * AdaptiveUiSheetCoordinator}.
     */
    interface Presenter {
        /** Hands the entry to its container, which may queue it. */
        void show();

        /**
         * Closes the entry if it is showing or queued; the presenter reports the dismissal.
         *
         * @param reason Why the entry is closed.
         * @param animate Whether to animate, where the container supports it.
         */
        void hide(@DismissReason int reason, boolean animate);

        /**
         * Whether the container is presenting the entry right now, rather than holding it in its
         * queue (not shown yet, or put back after it was shown). Closing the current entry lets the
         * container present the next one in its queue.
         */
        boolean isCurrent();
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final BottomSheetController mBottomSheetController;

    /** The presenter of every entry that is showing or queued. */
    private final Map<AdaptiveUiEntry, Presenter> mPresenters = new HashMap<>();

    /**
     * Entries not shown yet: from {@link #show} until the container shows or dismisses them. An
     * entry here without a presenter is still in its observer's onWillShow.
     */
    private final Set<AdaptiveUiEntry> mPendingEntries = new HashSet<>();

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
        // Closing the entry a container is presenting makes the container present the next one in
        // its queue, so close the current entries last: pending entries first, then shown entries
        // that a container put back in its queue. The collections are copied because close()
        // removes entries from them.
        for (AdaptiveUiEntry entry : new ArrayList<>(mPendingEntries)) {
            close(entry, DismissReason.TAB_CLOSED, /* animate= */ false);
        }
        List<AdaptiveUiEntry> currentEntries = new ArrayList<>();
        for (Map.Entry<AdaptiveUiEntry, Presenter> item : new ArrayList<>(mPresenters.entrySet())) {
            if (item.getValue().isCurrent()) {
                currentEntries.add(item.getKey());
            } else {
                close(item.getKey(), DismissReason.TAB_CLOSED, /* animate= */ false);
            }
        }
        for (AdaptiveUiEntry entry : currentEntries) {
            close(entry, DismissReason.TAB_CLOSED, /* animate= */ false);
        }
        AdaptiveUiControllerProvider.detach(this);
    }

    @Override
    public void show(AdaptiveUiEntry entry) {
        assert !mDestroyed : "A destroyed controller should not have #show invoked on it.";
        // An entry that is already pending or showing stays in its current container.
        if (mDestroyed || mPendingEntries.contains(entry) || mPresenters.containsKey(entry)) {
            return;
        }

        @AdaptiveUiContainer int container = selectContainer(entry.getAllowedContainers());
        AdaptiveUiObserver observer = entry.getObserver();
        // Added before onWillShow, so a hide() or destroy() from the observer finds the entry.
        mPendingEntries.add(entry);
        // Fires before the presenter binds the entry's View into the container.
        if (observer != null) observer.onWillShow(container);
        // The observer may have hidden the entry (or destroyed this controller) in onWillShow, and
        // close() already reported the dismissal; or it hid and showed it again, and that nested
        // show() already created the presenter.
        if (!mPendingEntries.contains(entry) || mPresenters.containsKey(entry)) return;

        Runnable onShown =
                () -> {
                    if (mDestroyed) return;
                    mPendingEntries.remove(entry);
                    notifyShown(entry);
                };
        Callback<Integer> onDismissed =
                reason -> {
                    mPresenters.remove(entry);
                    mPendingEntries.remove(entry);
                    notifyDismissed(entry, reason);
                };
        Presenter presenter =
                container == AdaptiveUiContainer.MODAL_DIALOG
                        ? new AdaptiveUiDialogCoordinator(
                                mContext,
                                entry.getPropertyModel(),
                                entry.getScope(),
                                mModalDialogManager,
                                onShown,
                                onDismissed)
                        : new AdaptiveUiSheetCoordinator(
                                mContext,
                                entry.getPropertyModel(),
                                entry.getScope(),
                                mBottomSheetController,
                                onShown,
                                onDismissed);
        mPresenters.put(entry, presenter);
        presenter.show();
    }

    @Override
    public void hide(AdaptiveUiEntry entry, @DismissReason int reason) {
        close(entry, reason, /* animate= */ true);
    }

    /**
     * Closes {@code entry} if it is showing or queued. An entry hidden from its observer's
     * onWillShow has no presenter yet, so it is dismissed here directly.
     */
    private void close(AdaptiveUiEntry entry, @DismissReason int reason, boolean animate) {
        Presenter presenter = mPresenters.get(entry);
        if (presenter != null) {
            presenter.hide(reason, animate);
        } else if (mPendingEntries.remove(entry)) {
            notifyDismissed(entry, reason);
        }
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
