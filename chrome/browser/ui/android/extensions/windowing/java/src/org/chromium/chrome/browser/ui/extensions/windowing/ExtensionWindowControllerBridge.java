// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/**
 * Provides access to cross-platform extension windowing code for Java.
 *
 * <p>This interface extends {@link ChromeAndroidTaskFeature} so that the lifecycle of an instance
 * will be in sync with that of a {@code ChromeAndroidTask}.
 */
@NullMarked
public interface ExtensionWindowControllerBridge extends ChromeAndroidTaskFeature {}
