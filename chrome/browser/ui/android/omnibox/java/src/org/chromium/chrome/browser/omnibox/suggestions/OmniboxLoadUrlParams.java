// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.ui.base.PageTransition;

import java.util.Map;

/** Holds parameters for AutocompleteDelegate.LoadUrl. */
@NullMarked
public class OmniboxLoadUrlParams {
    public final String url;
    public final @PageTransition int transitionType;
    public final long inputStartTimestamp;
    public final boolean openInNewTab;
    public final boolean openInNewWindow;
    public final byte @Nullable [] postData;
    public final Map<String, String> extraHeaders;
    public final @Nullable AutocompleteLoadCallback callback;

    private OmniboxLoadUrlParams(
            String url,
            @PageTransition int transitionType,
            long inputStartTimestamp,
            boolean openInNewTab,
            boolean openInNewWindow,
            byte @Nullable [] postData,
            Map<String, String> extraHeaders,
            @Nullable AutocompleteLoadCallback callback) {
        this.url = url;
        this.transitionType = transitionType;
        this.inputStartTimestamp = inputStartTimestamp;
        this.openInNewTab = openInNewTab;
        this.openInNewWindow = openInNewWindow;
        this.postData = postData;
        this.extraHeaders = extraHeaders;
        this.callback = callback;
    }

    /** The builder class for {@link OmniboxLoadUrlParams}. */
    public static class Builder {
        public String url;
        public @PageTransition int transitionType;
        public long inputStartTimestamp;
        public boolean openInNewTab;
        public boolean openInNewWindow;
        public byte @Nullable [] postData;
        public Map<String, String> extraHeaders = Map.of();
        public @Nullable AutocompleteLoadCallback callback;

        /**
         * @param url the URL will be loaded.
         * @param transitionType One of PAGE_TRANSITION static constants in ContentView.
         */
        public Builder(String url, @PageTransition int transitionType) {
            this.url = url;
            this.transitionType = transitionType;
        }

        /**
         * @param inputStartTimestamp the timestamp of the event in the location bar that triggered
         *     this URL load.
         */
        public Builder setInputStartTimestamp(long inputStartTimestamp) {
            this.inputStartTimestamp = inputStartTimestamp;
            return this;
        }

        /**
         * Set Whether the URL will be loaded in a new tab.
         *
         * @param openInNewTab Whether the URL will be loaded in a new tab.
         */
        public Builder setOpenInNewTab(boolean openInNewTab) {
            this.openInNewTab = openInNewTab;
            return this;
        }

        /**
         * Set Whether the URL will be loaded in a new window.
         *
         * @param openInNewTab Whether the URL will be loaded in a new window.
         */
        public Builder setOpenInNewWindow(boolean openInNewWindow) {
            this.openInNewWindow = openInNewWindow;
            return this;
        }

        /**
         * Set the post data of this load, and its type.
         *
         * @param postData Post data for this http post load.
         */
        public Builder setPostData(byte @Nullable [] postData) {
            this.postData = postData;
            return this;
        }

        /**
         * Set the extra headers for this navigation.
         *
         * @param extraHeaders Extra headers to be included with the HTTP request.
         */
        public Builder setExtraHeaders(Map<String, String> extraHeaders) {
            this.extraHeaders = extraHeaders;
            return this;
        }

        /**
         * Specify callback to be invoked once the URL is loaded.
         *
         * @param callback The callback to be invoked.
         */
        public Builder setAutocompleteLoadCallback(AutocompleteLoadCallback callback) {
            this.callback = callback;
            return this;
        }

        /** Builds the OmniboxLoadUrlParams. */
        public OmniboxLoadUrlParams build() {
            return new OmniboxLoadUrlParams(
                    url,
                    transitionType,
                    inputStartTimestamp,
                    openInNewTab,
                    openInNewWindow,
                    postData,
                    extraHeaders,
                    callback);
        }
    }
}
