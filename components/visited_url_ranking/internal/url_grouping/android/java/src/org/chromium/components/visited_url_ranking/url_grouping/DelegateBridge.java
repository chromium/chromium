// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;

import org.chromium.base.ObserverList;

/**
 * A wrapper for GroupSuggestionsService.Delegate
 *
 * <p>Hosts all the Java delegates of the service. Receives all the notifications from the native
 * counterpart GroupSuggestionsServiceAndroid and then notifies all the Java observers. NOTE: This
 * observer is not registered to the Java GroupSuggestionsService, this implements the
 * GroupSuggestionsService.Delegate only for readability. The native delegate is registered to the
 * native service.
 */
public class DelegateBridge implements GroupSuggestionsService.Delegate {
    ObserverList<GroupSuggestionsService.Delegate> mJavaDelegates = new ObserverList<>();

    public DelegateBridge() {}

    /** Register a new delegate. Each delegate can be registered only once */
    public void registerDelegate(GroupSuggestionsService.Delegate delegate) {
        mJavaDelegates.addObserver(delegate);
    }

    /** Unregister an registered delegate. Ignores if an delegate is not in the list. */
    public void unregisterDelegate(GroupSuggestionsService.Delegate delegate) {
        mJavaDelegates.removeObserver(delegate);
    }

    @CalledByNative
    @Override
    public void showSuggestion() {
        // TODO(crbug.com/397221723): Only show suggestions to corresponding window.
        for (GroupSuggestionsService.Delegate javaDelegate : mJavaDelegates) {
            javaDelegate.showSuggestion();
        }
    }

    @CalledByNative
    @Override
    public void onDumpStateForFeedback(String dumpState) {
        for (GroupSuggestionsService.Delegate javaDelegate : mJavaDelegates) {
            javaDelegate.onDumpStateForFeedback(dumpState);
        }
    }
}
