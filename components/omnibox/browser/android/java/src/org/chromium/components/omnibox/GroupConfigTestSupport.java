// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.components.omnibox.GroupsProto.GroupSection;

/**
 * Utility class for tests utilizing GroupConfigs that aids constructing of AutocompleteResult
 * definitions.
 */
@VisibleForTesting
public class GroupConfigTestSupport {
    /** Invalid group. */
    public static GroupConfig SECTION_INVALID = GroupConfig.newBuilder().build();

    /** Verbatim Match group typically found in zero-suggest contexts with no header text. */
    public static GroupConfig SECTION_VERBATIM =
            GroupConfig.newBuilder().setSection(GroupSection.SECTION_MOBILE_VERBATIM).build();

    /** Clipboard group, that shares the section with Verbatim Match. */
    public static GroupConfig SECTION_CLIPBOARD =
            GroupConfig.newBuilder().setSection(GroupSection.SECTION_MOBILE_CLIPBOARD).build();

    /** Most Visited Tiles group, with no header text and horizontal layout direction. */
    public static GroupConfig SECTION_MOST_VISITED =
            GroupConfig.newBuilder()
                    .setSection(GroupSection.SECTION_MOBILE_MOST_VISITED)
                    .setRenderType(GroupConfig.RenderType.HORIZONTAL)
                    .build();

    /** Suggestions with no headers, expanded, section 1. */
    public static GroupConfig SECTION_1_EXPANDED_NO_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_1, "", true);

    /** Suggestions with no headers, collapsed, section 1. */
    public static GroupConfig SECTION_1_COLLAPSED_NO_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_1, "", false);

    /** Suggestions with headers, expanded, section 2. */
    public static GroupConfig SECTION_2_EXPANDED_WITH_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_2, "Section #2", true);

    /** Suggestions with headers, collapsed, section 2. */
    public static GroupConfig SECTION_2_COLLAPSED_WITH_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_2, "Section #2", false);

    /** Suggestions with headers, expanded, section 3. */
    public static GroupConfig SECTION_3_EXPANDED_WITH_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_3, "Section #3", true);

    /** Suggestions with headers, collapsed, section 3. */
    public static GroupConfig SECTION_3_COLLAPSED_WITH_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_3, "Section #3", false);
    /**
     * Create a simple GroupConfig instance with supplied text and visibility.
     *
     * @param section The target section for this GroupConfig.
     * @param headerText The header text to apply to group config.
     * @param isVisible Whether the newly built group is expanded.
     * @return Newly constructed GroupConfig.
     */
    public static GroupConfig buildGroupConfig(
            @NonNull GroupSection section, @NonNull String headerText, boolean isVisible) {
        return GroupConfig.newBuilder()
                .setSection(section)
                .setHeaderText(headerText)
                .setVisibility(isVisible ? GroupConfig.Visibility.DEFAULT_VISIBLE
                                         : GroupConfig.Visibility.HIDDEN)
                .build();
    }
}
