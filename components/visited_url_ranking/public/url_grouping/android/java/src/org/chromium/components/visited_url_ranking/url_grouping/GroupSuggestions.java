// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Information about a list of group suggestions. */
@JNINamespace("visited_url_ranking")
@NullMarked
public class GroupSuggestions {
    public final @Nullable List<GroupSuggestion> groupSuggestions;

    public GroupSuggestions(List<GroupSuggestion> suggestions) {
        this.groupSuggestions = suggestions;
    }

    @CalledByNative
    private static GroupSuggestions createGroupSuggestions(
            @JniType("std::vector") List<GroupSuggestion> suggestions) {
        return new GroupSuggestions(suggestions);
    }
}
