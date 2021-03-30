// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from './async_job_queue.js';
import {assertInstanceof} from './chrome_util.js';

/**
 * @type {!Map<!HTMLElement, !AsyncJobQueue>}
 */
const jobQueueMap = new Map();

/**
 * Gets the animation job queue for the element.
 * @param {!HTMLElement} el
 * @return {!AsyncJobQueue}
 */
function getQueueFor(el) {
  if (!jobQueueMap.has(el)) {
    jobQueueMap.set(el, new AsyncJobQueue());
  }
  return jobQueueMap.get(el);
}

/**
 * Gets all the animations running or pending on the element and its
 * pseudo-elements.
 * TODO(b/176879728): Remove @suppress once we fix the getAnimations() extern
 * in upstream Closure compiler.
 * @suppress {checkTypes}
 * @param {!HTMLElement} el
 * @return {!Array<!Animation>}
 */
function getAnimations(el) {
  return el.getAnimations({subtree: true})
      .filter((a) => assertInstanceof(a.effect, KeyframeEffect).target === el);
}

/**
 * Cancels the running animation on the element, if any.
 * @param {!HTMLElement} el
 * @return {!Promise} Promise resolved when the animation is cancelled.
 */
export async function cancel(el) {
  getAnimations(el).forEach((a) => a.cancel());
  await getQueueFor(el).flush();
}

/**
 * Animates the element once by applying the "animate" class. If the animation
 * is already running, the previous one would be cancelled first.
 * @param {!HTMLElement} el
 * @return {!Promise} Promise resolved when the animation is settled.
 */
export function play(el) {
  cancel(el);
  const queue = getQueueFor(el);
  const job = async () => {
    /**
     * Force repaint before applying the animation.
     * @suppress {suspiciousCode}
     */
    el.offsetWidth;
    el.classList.add('animate');
    await Promise.allSettled(getAnimations(el).map((a) => a.finished));
    el.classList.remove('animate');
  };
  return queue.push(job);
}
