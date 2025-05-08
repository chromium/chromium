// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;

/**
 * A wrapper for GroupSuggestionsService.Delegate
 *
 * <p>Hosts all the Java delegates of the service. Receives all the notifications from the native
 * counterpart GroupSuggestionsServiceAndroid and then notifies all the Java observers. NOTE: This
 * observer is not registered to the Java GroupSuggestionsService. The native delegate is registered
 * to the native service.
 */
@NullMarked
public class DelegateBridge {
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
    public void showSuggestion(
            GroupSuggestions groupSuggestions, JniOnceCallback<UserResponseMetadata> callback) {
        // TODO(crbug.com/397221723): Only show suggestions to corresponding window.
        if (mJavaDelegates.isEmpty()) {
            int suggestion_id;
            if (groupSuggestions == null
                    || groupSuggestions.groupSuggestions == null
                    || groupSuggestions.groupSuggestions.isEmpty()) {
                suggestion_id = 0;
            } else {
                suggestion_id = groupSuggestions.groupSuggestions.get(0).suggestionId;
            }
            // TODO(crbug.com/397221723): Only return REJECTED when create_suggestions_promotion_ui
            // is disabled; currently having no Java delegates means that
            // create_suggestions_promotion_ui is disabled.
            callback.onResult(new UserResponseMetadata(suggestion_id, UserResponse.REJECTED));
            return;
        }
        for (GroupSuggestionsService.Delegate javaDelegate : mJavaDelegates) {
            javaDelegate.showSuggestion(groupSuggestions, (result) -> callback.onResult(result));
        }
    }

    @CalledByNative
    public void onDumpStateForFeedback(String dumpState) {
        for (GroupSuggestionsService.Delegate javaDelegate : mJavaDelegates) {
            javaDelegate.onDumpStateForFeedback(dumpState);
        }
    }
}
