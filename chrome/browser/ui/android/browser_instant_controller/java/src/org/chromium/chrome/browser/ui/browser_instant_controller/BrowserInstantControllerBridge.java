// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_instant_controller;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/**
 * Bridge for the native {@code BrowserInstantController}, which reloads a browser window's New Tab
 * Pages when the default search engine changes.
 *
 * <p>This interface extends {@link ChromeAndroidTaskFeature} so that the lifecycle of an instance
 * will be in sync with that of a {@code ChromeAndroidTask}.
 */
@NullMarked
public interface BrowserInstantControllerBridge extends ChromeAndroidTaskFeature {}
