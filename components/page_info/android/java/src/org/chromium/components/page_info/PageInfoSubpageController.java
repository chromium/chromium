// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for a page info subpage controller. */
@NullMarked
public interface PageInfoSubpageController {
    /** Returns a title string for the page info subpage. */
    @Nullable String getSubpageTitle();

    /** Creates and returns a personalized subview to be used inside of the page info subpage. */
    @Nullable View createViewForSubpage(ViewGroup parent);

    /** Returns the current subpage view if it exists. */
    @Nullable View getCurrentSubpageView();

    /** Called after the subpage closes in order to perform any necessary cleanup. */
    void onSubpageRemoved();

    /** Clears data related to the subpage. */
    void clearData();

    /**
     * Notifies the subpage that they should update their PageInfoRowView if they have changes since
     * the last time.
     */
    void updateRowIfNeeded();

    /** Notifies the subpage that it should update it's data since it is potentially stale. */
    void updateSubpageIfNeeded();
}
