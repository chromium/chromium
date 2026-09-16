// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;

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
     * Returns whether the given tool mode is any sort of image generation tool. Note that
     * TOOL_MODE_IMAGE_GEN_SELFIE is deliberately excluded, matching getRequestTypeForToolMode,
     * which does not map it to IMAGE_GENERATION either.
     */
    public static boolean isImageGenTool(@ToolMode int toolMode) {
        return toolMode == ToolMode.TOOL_MODE_IMAGE_GEN
                || toolMode == ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD;
    }

    /**
     * @param requestType The current request type.
     * @param hasAttachments If there are any attachments.
     * @return The ToolMode for the given request type and attachment state.
     */
    public static @ToolMode int getToolModeForRequestType(
            @AutocompleteRequestType int requestType, boolean hasAttachments) {
        return switch (requestType) {
            case AutocompleteRequestType.IMAGE_GENERATION ->
                    hasAttachments
                            ? ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD
                            : ToolMode.TOOL_MODE_IMAGE_GEN;
            case AutocompleteRequestType.DEEP_SEARCH -> ToolMode.TOOL_MODE_DEEP_SEARCH;
            case AutocompleteRequestType.CANVAS -> ToolMode.TOOL_MODE_CANVAS;
            default -> ToolMode.TOOL_MODE_UNSPECIFIED;
        };
    }

    /**
     * @param toolMode The ToolMode value.
     * @return The AutocompleteRequestType for the given ToolMode.
     */
    public static @AutocompleteRequestType int getRequestTypeForToolMode(@ToolMode int toolMode) {
        return switch (toolMode) {
            case ToolMode.TOOL_MODE_IMAGE_GEN, ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD ->
                    AutocompleteRequestType.IMAGE_GENERATION;
            case ToolMode.TOOL_MODE_DEEP_SEARCH -> AutocompleteRequestType.DEEP_SEARCH;
            case ToolMode.TOOL_MODE_CANVAS -> AutocompleteRequestType.CANVAS;
            default -> AutocompleteRequestType.AI_MODE;
        };
    }
}
