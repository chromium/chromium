// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

/**
 * Interface for a page info subpage controller.
 */
public interface PageInfoSubpageController {
    /**
     * Returns a title string for the page info subpage.
     */
    String getSubpageTitle();

    /**
     * Returns a personalized subview to be used inside of the page info subpage.
     */
    View createViewForSubpage(ViewGroup parent);

    /**
     * Called after the subpage closes in order to perform any necessary cleanup.
     */
    void onSubpageRemoved();
}
