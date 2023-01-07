// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

/**
 * This interface is intended to be used by glue code to manage the lifecycle and external input to
 * the {@link LayoutManager} implementations.
 */
public interface ManagedLayoutManager extends LayoutManagerProvider.Unowned {
    /**
     * Clean up any state held by the layout manager to prepare for browser shutdown. The object
     * should not be used after this has been called.
     */
    void destroy();
}
