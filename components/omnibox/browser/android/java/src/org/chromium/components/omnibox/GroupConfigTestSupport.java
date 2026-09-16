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

    /** Query Tiles group, with no header text and horizontal layout direction. */
    public static GroupConfig SECTION_QUERY_TILES =
            GroupConfig.newBuilder()
                    .setSection(GroupSection.SECTION_MOBILE_QUERY_TILES)
                    .setRenderType(GroupConfig.RenderType.HORIZONTAL)
                    .build();

    /** Suggestions with no headers, section 1. */
    public static GroupConfig SECTION_1_NO_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_1, "");

    /** Suggestions with headers, section 2. */
    public static GroupConfig SECTION_2_WITH_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_2, "Section #2");

    /** Suggestions with headers, section 3. */
    public static GroupConfig SECTION_3_WITH_HEADER =
            buildGroupConfig(GroupSection.SECTION_REMOTE_ZPS_3, "Section #3");

    /**
     * Create a simple GroupConfig instance with supplied text and visibility.
     *
     * @param section The target section for this GroupConfig.
     * @param headerText The header text to apply to group config.
     * @param isVisible Whether the newly built group is expanded.
     * @return Newly constructed GroupConfig.
     */
    public static GroupConfig buildGroupConfig(
            @NonNull GroupSection section, @NonNull String headerText) {
        return GroupConfig.newBuilder().setSection(section).setHeaderText(headerText).build();
    }
}
