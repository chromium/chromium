// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabObserver;

/** JNI bridge for a tab-scoped {@code SidePanelRegistry}. */
@NullMarked
public interface TabScopedSidePanelRegistryBridge extends TabObserver {}
