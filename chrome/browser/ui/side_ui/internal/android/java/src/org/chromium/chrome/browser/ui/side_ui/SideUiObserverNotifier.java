// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.Transition;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiShowability;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;

import java.util.ArrayList;
import java.util.List;

/** Manages all {@link SideUiObserver}s and invokes their APIs. */
@DoNotMock
@NullMarked
final class SideUiObserverNotifier {

    private final List<SideUiObserver> mObservers = new ArrayList<>();

    private @Nullable SideUiShowability mLastSideUiShowability;

    /** See {@link SideUiStateProvider#addObserver(SideUiObserver)}. */
    void addObserver(SideUiObserver observer) {
        mObservers.add(observer);
    }

    /** See {@link SideUiStateProvider#removeObserver(SideUiObserver)}. */
    void removeObserver(SideUiObserver observer) {
        mObservers.remove(observer);
    }

    /**
     * Invokes {@link SideUiObserver#onShowableSideUisUpdated} if {@link SideUiShowability} has
     * changed.
     */
    void notifySideUiShowability(SideUiShowability showability) {
        if (showability.equals(mLastSideUiShowability)) {
            return;
        }

        mLastSideUiShowability = showability;

        for (var observer : mObservers) {
            observer.onShowableSideUisUpdated(showability);
        }
    }

    /**
     * Invokes {@link SideUiObserver#onPreSideUiSpecsChange}.
     *
     * @return All {@link Transition}s returned by {@link SideUiObserver}s.
     */
    List<Transition> notifyPreSideUiSpecsChange(
            SideUiSpecs newSideUiSpecs, UiUpdateRequest request) {
        List<Transition> transitions = new ArrayList<>();
        for (var observer : mObservers) {
            @Nullable Transition transition =
                    observer.onPreSideUiSpecsChange(newSideUiSpecs, request);
            if (transition != null) {
                transitions.add(transition);
            }
        }

        return transitions;
    }

    /** Invokes {@link SideUiObserver#onTransitionBegun}. */
    void notifyTransitionBegun(SideUiSpecs newSideUiSpecs, UiUpdateRequest request) {
        for (var observer : mObservers) {
            observer.onTransitionBegun(newSideUiSpecs, request);
        }
    }

    /** Invokes {@link SideUiObserver#onTransitionEnded}. */
    void notifyTransitionEnded(SideUiSpecs newSideUiSpecs, UiUpdateRequest request) {
        for (var observer : mObservers) {
            observer.onTransitionEnded(newSideUiSpecs, request);
        }
    }

    /** Invokes {@link SideUiObserver#onSideUiSpecsChanged}. */
    void notifySideUiSpecsChanged(SideUiSpecs newSideUiSpecs, UiUpdateRequest request) {
        for (var observer : mObservers) {
            observer.onSideUiSpecsChanged(newSideUiSpecs, request);
        }
    }
}
