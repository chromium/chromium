// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;

/** Parameters for attaching a {@link WebContents} to a {@link ThinWebView}. */
@NullMarked
public class ThinWebViewAttachParams {
    public final @Nullable WebContentsDelegateAndroid webContentsDelegate;
    public final @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory;
    public final @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate;
    public final boolean supportTheming;
    public final boolean enableBrowserAutofill;

    private ThinWebViewAttachParams(
            @Nullable WebContentsDelegateAndroid webContentsDelegate,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
            boolean supportTheming,
            boolean enableBrowserAutofill) {
        this.webContentsDelegate = webContentsDelegate;
        this.contextMenuPopulatorFactory = contextMenuPopulatorFactory;
        this.selectionDropdownMenuDelegate = selectionDropdownMenuDelegate;
        this.supportTheming = supportTheming;
        this.enableBrowserAutofill = enableBrowserAutofill;
    }

    /** Builder for {@link ThinWebViewAttachParams}. */
    public static class Builder {
        @Nullable private WebContentsDelegateAndroid mWebContentsDelegate;
        @Nullable private ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
        @Nullable private SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;
        private boolean mSupportTheming;
        private boolean mEnableBrowserAutofill = true;

        public Builder() {}

        /**
         * Sets the {@link WebContentsDelegateAndroid} to be used.
         *
         * @param webContentsDelegate The delegate.
         * @return This builder.
         */
        public Builder setWebContentsDelegate(
                @Nullable WebContentsDelegateAndroid webContentsDelegate) {
            mWebContentsDelegate = webContentsDelegate;
            return this;
        }

        /**
         * Sets the {@link ContextMenuPopulatorFactory} to be used.
         *
         * @param contextMenuPopulatorFactory The factory.
         * @return This builder.
         */
        public Builder setContextMenuPopulatorFactory(
                @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory) {
            mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
            return this;
        }

        /**
         * Sets the {@link SelectionDropdownMenuDelegate} to be used.
         *
         * @param selectionDropdownMenuDelegate The delegate.
         * @return This builder.
         */
        public Builder setSelectionDropdownMenuDelegate(
                @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate) {
            mSelectionDropdownMenuDelegate = selectionDropdownMenuDelegate;
            return this;
        }

        /**
         * Sets whether to support theming (e.g. night mode).
         *
         * @param supportTheming Whether theming is supported.
         * @return This builder.
         */
        public Builder setSupportTheming(boolean supportTheming) {
            mSupportTheming = supportTheming;
            return this;
        }

        /**
         * Sets whether browser autofill tab helper attachment should be enabled.
         *
         * @param enableBrowserAutofill Whether browser Autofill should be enabled.
         * @return This builder.
         */
        public Builder setEnableBrowserAutofill(boolean enableBrowserAutofill) {
            mEnableBrowserAutofill = enableBrowserAutofill;
            return this;
        }

        /**
         * Builds the {@link ThinWebViewAttachParams}.
         *
         * @return The built parameters.
         */
        public ThinWebViewAttachParams build() {
            return new ThinWebViewAttachParams(
                    mWebContentsDelegate,
                    mContextMenuPopulatorFactory,
                    mSelectionDropdownMenuDelegate,
                    mSupportTheming,
                    mEnableBrowserAutofill);
        }
    }
}
