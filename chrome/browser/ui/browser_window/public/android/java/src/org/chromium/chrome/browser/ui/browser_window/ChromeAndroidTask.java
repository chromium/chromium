// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents an Android window containing Chrome.
 *
 * <p>In Android, a window is a <i>Task</i>. However, depending on Task API availability in Android
 * framework, the implementation of this interface may rely on {@code ChromeActivity} as a
 * workaround to track windows.
 *
 * <p>The main difference between Task-based window tracking and {@code ChromeActivity}-based window
 * tracking is the lifecycle of a {@link ChromeAndroidTask} object, as the OS can kill {@code
 * ChromeActivity} when it's in the background, but keep its Task.
 *
 * <p>Example 1:
 *
 * <ul>
 *   <li>The user opens {@code ChromeActivity}.
 *   <li>The user then opens {@code SettingsActivity}, or an {@code Activity} not owned by Chrome.
 *   <li>If the OS decides to kill {@code ChromeActivity}, which is now in the background, {@code
 *       ChromeActivity}-based window tracking will lose the window, but the Task (window) is still
 *       alive and visible to the user.
 * </ul>
 *
 * <p>Example 2:
 *
 * <ul>
 *   <li>The user opens {@code ChromeActivity}.
 *   <li>The user moves the Task to the background.
 *   <li>If the OS decides to kill {@code ChromeActivity}, but keep its Task, {@code
 *       ChromeActivity}-based window tracking will lose the window, but the Task (window) can still
 *       be seen in "Recents" and restored by the user.
 * </ul>
 */
@NullMarked
public interface ChromeAndroidTask {}
