// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.progress

/** Interface for providing progress updates. */
interface ProgressProvider {
  /** Observer for progress updates. */
  interface Observer {
    /** Called when progress changes. */
    fun onProgressChanged(progress: Float)
  }

  /** Adds an observer. */
  fun addObserver(observer: Observer)

  /** Removes an observer. */
  fun removeObserver(observer: Observer)

  /** Triggers the progress updates. */
  fun triggerProgress()

  /** Cleans up resources. */
  fun destroy()
}
