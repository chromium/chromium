// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
        /**
         * Hands the entry to its container, which may queue it.
         *
         * @return {@code true} if the container is showing or taking over for the entry now; {@code
         *     false} if it was queued, suppressed, or dropped.
         */
        boolean show();

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
        // its queue, so close the entries waiting in a queue first (not shown yet, or put back
        // after they were shown) and the current ones last. The map is copied because close()
        // removes entries from it.
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
    public @Nullable @AdaptiveUiContainer Integer show(AdaptiveUiEntry entry) {
        assert !mDestroyed : "A destroyed controller should not have #show invoked on it.";
        // An entry that is already showing or queued stays in its current container.
        if (mDestroyed || mPresenters.containsKey(entry)) return null;

        @AdaptiveUiContainer int container = selectContainer(entry.getAllowedContainers());
        Set<AdaptiveUiEntry> shownEntries = new HashSet<>();
        Runnable onShown =
                () -> {
                    if (mDestroyed) return;
                    shownEntries.add(entry);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Android.AdaptiveUi.ContainerShown",
                            container,
                            AdaptiveUiContainer.COUNT);
                    notifyShown(entry, container);
                };
        Callback<Integer> onDismissed =
                reason -> {
                    mPresenters.remove(entry);
                    boolean dismissedBeforeShown = !shownEntries.remove(entry);
                    recordDismissalMetrics(container, reason, dismissedBeforeShown);
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
                                entry.isSheetUserInitiated(),
                                entry.getSheetUserCriticalFeature(),
                                mBottomSheetController,
                                onShown,
                                onDismissed);
        mPresenters.put(entry, presenter);
        boolean shownNow = presenter.show();
        if (mPresenters.containsKey(entry)) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.AdaptiveUi.EntryDeferred." + getContainerSuffix(container),
                    !shownEntries.contains(entry) && !shownNow);
        }
        return shownNow ? container : null;
    }

    @Override
    public void dismiss(AdaptiveUiEntry entry, @DismissReason int reason) {
        close(entry, reason, /* animate= */ true);
    }

    /** Closes {@code entry} if it is showing or queued. */
    private void close(AdaptiveUiEntry entry, @DismissReason int reason, boolean animate) {
        Presenter presenter = mPresenters.get(entry);
        if (presenter != null) presenter.hide(reason, animate);
    }

    /**
     * Picks the container for one {@link #show}: a dialog on desktop form factors and a sheet
     * elsewhere, falling back to the other one when the preferred one isn't allowed.
     */
    static @AdaptiveUiContainer int selectContainer(
            Set<@AdaptiveUiContainer Integer> allowedContainers) {
        boolean dialogAllowed = allowedContainers.contains(AdaptiveUiContainer.MODAL_DIALOG);
        boolean sheetAllowed = allowedContainers.contains(AdaptiveUiContainer.BOTTOM_SHEET);
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

    // LINT.IfChange(AdaptiveUiContainerSuffix)
    private static String getContainerSuffix(@AdaptiveUiContainer int container) {
        return container == AdaptiveUiContainer.MODAL_DIALOG ? "ModalDialog" : "BottomSheet";
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/histograms.xml:AdaptiveUiContainerSuffix)

    private static void recordDismissalMetrics(
            @AdaptiveUiContainer int container,
            @DismissReason int reason,
            boolean dismissedBeforeShown) {
        String histogram =
                dismissedBeforeShown
                        ? "Android.AdaptiveUi.DismissReasonBeforeShown."
                        : "Android.AdaptiveUi.DismissReason.";
        RecordHistogram.recordEnumeratedHistogram(
                histogram + getContainerSuffix(container), reason, DismissReason.COUNT);
    }

    private static void notifyShown(AdaptiveUiEntry entry, @AdaptiveUiContainer int container) {
        AdaptiveUiObserver observer = entry.getObserver();
        if (observer != null) observer.onShown(container);
    }

    private static void notifyDismissed(AdaptiveUiEntry entry, @DismissReason int reason) {
        AdaptiveUiObserver observer = entry.getObserver();
        if (observer != null) observer.onDismissed(reason);
    }
}
