// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

/** Utility methods for managing ToolModes and their relationship with RequestTypes. */
@NullMarked
public class ToolModeUtils {
    private ToolModeUtils() {}

    /** Returns whether the given request type is any sort of specialized AI request. */
    public static boolean isAimRequest(@AutocompleteRequestType int requestType) {
        return requestType == AutocompleteRequestType.AI_MODE
                || requestType == AutocompleteRequestType.IMAGE_GENERATION
                || requestType == AutocompleteRequestType.DEEP_SEARCH
                || requestType == AutocompleteRequestType.CANVAS;
    }

    /** Returns whether the given request type is a conventional request. */
    public static boolean isConventionalRequest(@AutocompleteRequestType int requestType) {
        return requestType == AutocompleteRequestType.SEARCH
                || requestType == AutocompleteRequestType.SEARCH_PREFETCH;
    }

    /**
     * @param requestType The current request type.
     * @param hasAttachments If there are any attachments.
     * @return The ToolMode for the given request type and attachment state.
     */
    public static /* ToolMode */ int getToolModeForRequestType(
            @AutocompleteRequestType int requestType, boolean hasAttachments) {
        return switch (requestType) {
            case AutocompleteRequestType.IMAGE_GENERATION ->
                    hasAttachments
                            ? ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE
                            : ToolMode.TOOL_MODE_IMAGE_GEN_VALUE;
            case AutocompleteRequestType.DEEP_SEARCH -> ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE;
            case AutocompleteRequestType.CANVAS -> ToolMode.TOOL_MODE_CANVAS_VALUE;
            default -> ToolMode.TOOL_MODE_UNSPECIFIED_VALUE;
        };
    }

    /**
     * @param toolMode The ToolMode value.
     * @return The AutocompleteRequestType for the given ToolMode.
     */
    public static @AutocompleteRequestType int getRequestTypeForToolMode(int toolMode) {
        return switch (toolMode) {
            case ToolMode.TOOL_MODE_IMAGE_GEN_VALUE, ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE ->
                    AutocompleteRequestType.IMAGE_GENERATION;
            case ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE -> AutocompleteRequestType.DEEP_SEARCH;
            case ToolMode.TOOL_MODE_CANVAS_VALUE -> AutocompleteRequestType.CANVAS;
            default -> AutocompleteRequestType.AI_MODE;
        };
    }
}
