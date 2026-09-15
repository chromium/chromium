// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * Immutable description of the AI Mode entry point offered by a single search engine.
 *
 * <p>Java counterpart of the native {@code AiModeButtonUiConfig} struct (see
 * //components/search_engines/ai_mode_button_service.h). Instances are built exclusively by native
 * code; the native {@code id} field is deliberately not mirrored, as it only identifies the search
 * engine the config was resolved from, which the caller already knows.
 *
 * <p>Google is the only engine whose entry point is rendered from built-in assets and navigated to
 * via the regular search engine plumbing, so {@link #faviconUrl}, {@link #navigationUrl} and {@link
 * #navigationUrlEmpty} are empty for Google and non-empty for every third party engine.
 */
@NullMarked
public final class AiModeButtonUiConfig {
    /** Label shown on the AI Mode button. {@code IDS_AI_MODE_ENTRYPOINT_LABEL}, e.g. "AI Mode". */
    public final String text;

    /**
     * Hover / long press tooltip. Unlike {@link #text}, this is a full sentence naming the host
     * search engine for Google ({@code IDS_AI_MODE_ENTRYPOINT_TOOLTIP_1P}, e.g. "Ask AI Mode in
     * Google Search") and just the provider for third parties ({@code
     * IDS_AI_MODE_ENTRYPOINT_TOOLTIP_3P}, e.g. "Ask AI Mode").
     */
    public final String tooltip;

    /**
     * Accessibility label announced when the AI Mode button gains focus. {@code
     * IDS_AI_MODE_ENTRYPOINT_ACC_FOCUSED}, e.g. "AI Mode button, press Enter to ask AI Mode".
     */
    public final String a11yLabel;

    /**
     * Label of the context menu item pinning the AI Mode button. {@code
     * IDS_AI_MODE_ENTRYPOINT_CONTEXT_MENU_SHOW}, e.g. "Always show AI Mode".
     */
    public final String contextMenuLabel;

    /**
     * Omnibox hint text shown while the AI Mode entry point is engaged. {@code
     * IDS_AI_MODE_OMNIBOX_PLACEHOLDER}, e.g. "Press tab then enter to ask AI Mode".
     *
     * <p>Note this is the desktop variant. Android clients likely want {@code
     * IDS_AI_MODE_OMNIBOX_PLACEHOLDER_ANDROID}, which prefixes a {@code <tab_key>} placeholder for
     * the rendered key glyph; see crbug.com/561690870.
     */
    public final String placeholderText;

    /** Favicon representing the AI Mode provider. Empty for Google. */
    public final GURL faviconUrl;

    /**
     * Search URL template invoked when the user engages AI Mode with query terms. Modeled as a
     * String rather than a {@link GURL}, because it carries an unsubstituted {@code {searchTerms}}
     * placeholder and is therefore not directly navigable. Empty for Google.
     */
    public final String navigationUrl;

    /** URL navigated to when the user engages AI Mode with no query terms. Empty for Google. */
    public final GURL navigationUrlEmpty;

    @VisibleForTesting
    @CalledByNative
    public AiModeButtonUiConfig(
            @JniType("std::u16string") String text,
            @JniType("std::u16string") String tooltip,
            @JniType("std::u16string") String a11yLabel,
            @JniType("std::u16string") String contextMenuLabel,
            @JniType("std::u16string") String placeholderText,
            @JniType("GURL") GURL faviconUrl,
            @JniType("std::string") String navigationUrl,
            @JniType("GURL") GURL navigationUrlEmpty) {
        this.text = text;
        this.tooltip = tooltip;
        this.a11yLabel = a11yLabel;
        this.contextMenuLabel = contextMenuLabel;
        this.placeholderText = placeholderText;
        this.faviconUrl = faviconUrl;
        this.navigationUrl = navigationUrl;
        this.navigationUrlEmpty = navigationUrlEmpty;
    }
}
