// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.progress

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.update

/** Mediator that manages [ProgressUiState] based on progress provided by a [ProgressProvider]. */
class ProgressMediator(
  uiState: MutableStateFlow<ProgressUiState>,
  provider: ProgressProvider,
) : ProgressProvider.Observer {
  private val mUiState = uiState
  private val mProvider = provider

  init {
    mProvider.addObserver(this)
  }

  override fun onProgressChanged(progress: Float) {
    mUiState.update {
      it.copy(
        progressFraction = progress,
        isRunning = progress > 0f && progress < 1f,
      )
    }
  }

  /** Destroys the mediator and its dependencies. */
  fun destroy() {
    mProvider.removeObserver(this)
  }
}
