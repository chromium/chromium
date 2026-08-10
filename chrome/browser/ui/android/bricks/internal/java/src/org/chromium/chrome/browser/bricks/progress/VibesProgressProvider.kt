// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.progress

import kotlin.random.Random
import kotlin.time.Duration.Companion.milliseconds
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * [ProgressProvider] that provides progress updates based on vibes alone.
 *
 * It emits progress updates at random intervals with random increments, simulating a very moody and
 * vibe-dependent progress.
 */
class VibesProgressProvider(mainDispatcher: CoroutineDispatcher = Dispatchers.Main) :
  ProgressProvider {
  private val mMainDispatcher = mainDispatcher
  private val mObservers = mutableListOf<ProgressProvider.Observer>()
  private val mScope = CoroutineScope(SupervisorJob() + mMainDispatcher)
  private var mJob: Job? = null

  override fun addObserver(observer: ProgressProvider.Observer) {
    mObservers.add(observer)
  }

  override fun removeObserver(observer: ProgressProvider.Observer) {
    mObservers.remove(observer)
  }

  override fun triggerProgress() {
    mJob?.cancel()
    mJob = mScope.launch {
      var progress = 0f
      notifyObservers(progress)
      while (progress < 1f) {
        // Delay between 100ms and 800ms (vibe check latency)
        delay(Random.nextLong(100, 800).milliseconds)
        // Progress increment between 5% and 20% (vibe intensity)
        val increment = Random.nextFloat() * 0.15f + 0.05f
        progress = (progress + increment).coerceAtMost(1f)
        notifyObservers(progress)
      }
    }
  }

  override fun destroy() {
    mScope.cancel()
  }

  private fun notifyObservers(progress: Float) {
    mObservers.forEach { it.onProgressChanged(progress) }
  }
}
