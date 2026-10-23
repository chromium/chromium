// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Shared state and layout constants for the Fusebox component. */
@NullMarked
public @interface Fusebox {
    /** Visual state of the Fusebox toolbar controls. */
    @IntDef({FuseboxState.DISABLED, FuseboxState.COMPACT, FuseboxState.EXPANDED})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    @interface FuseboxState {
        int DISABLED = 0;
        int COMPACT = 1;
        int EXPANDED = 2;
    }

    /** Layout mode for the Fusebox UI. */
    @IntDef({FuseboxLayoutMode.TOOLBAR, FuseboxLayoutMode.SUGGESTIONS_POPOVER})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    @interface FuseboxLayoutMode {
        // The Fusebox UI is embedded in the toolbar, separate from associated suggestions.
        int TOOLBAR = 1;
        // The Fusebox UI is intermingled with associated suggestions in a single popover window.
        int SUGGESTIONS_POPOVER = 2;
    }

    /** Display state of the Fusebox context popup. */
    @IntDef({PopupState.HIDDEN, PopupState.FLOATING, PopupState.BOTTOM})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    @interface PopupState {
        int HIDDEN = 0;
        int FLOATING = 1;
        int BOTTOM = 2;
    }

    /** Where the "Add current tab" button is placed within the popup. */
    @IntDef({
        CurrentTabPlacement.ABOVE_ATTACHMENTS,
        CurrentTabPlacement.WITH_ATTACHMENTS,
        CurrentTabPlacement.BELOW_ATTACHMENTS,
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    @interface CurrentTabPlacement {
        int ABOVE_ATTACHMENTS = 0;
        int WITH_ATTACHMENTS = 1;
        int BELOW_ATTACHMENTS = 2;
    }
}
