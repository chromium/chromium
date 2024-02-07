// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.ui.base.PageTransition;

/** Holds parameters for AutocompleteDelegate.LoadUrl. */
public class OmniboxLoadUrlParams {
    public final String url;
    public final @PageTransition int transitionType;
    public final long inputStartTimestamp;
    public final boolean openInNewTab;
    public final @Nullable byte[] postData;
    public final @Nullable String postDataType;
    public final @Nullable AutocompleteLoadCallback callback;

    private OmniboxLoadUrlParams(
            String url,
            @PageTransition int transitionType,
            long inputStartTimestamp,
            boolean openInNewTab,
            @Nullable byte[] postData,
            @Nullable String postDataType,
            @Nullable AutocompleteLoadCallback callback) {
        this.url = url;
        this.transitionType = transitionType;
        this.inputStartTimestamp = inputStartTimestamp;
        this.openInNewTab = openInNewTab;
        this.postData = postData;
        this.postDataType = postDataType;
        this.callback = callback;
    }

    /** The builder class for {@link OmniboxLoadUrlParams}. */
    public static class Builder {
        public String url;
        public @PageTransition int transitionType;
        public long inputStartTimestamp;
        public boolean openInNewTab;
        public @Nullable byte[] postData;
        public @Nullable String postDataType;
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
         * Set Whether the URL will be loaded in a new tab..
         *
         * @param openInNewTab Whether the URL will be loaded in a new tab..
         */
        public Builder setOpenInNewTab(boolean openInNewTab) {
            this.openInNewTab = openInNewTab;
            return this;
        }

        /**
         * Set the post data of this load, and its type.
         *
         * @param postData Post data for this http post load.
         * @param postDataType Post data type for this http post load.
         */
        public Builder setpostDataAndType(byte[] postData, String postDataType) {
            this.postData = postData;
            this.postDataType = postDataType;
            return this;
        }

        /**
         * Set the callback of this loa.
         *
         * @param callback The callback will be called once the url is loaded.
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
                    postData,
                    postDataType,
                    callback);
        }
    }
}
