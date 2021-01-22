// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from './async_job_queue.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * @type {!Map<!HTMLAudioElement, !AsyncJobQueue>}
 */
const jobQueueMap = new Map();

/**
 * Gets the audio job queue for the element.
 * @param {!HTMLAudioElement} el
 * @return {!AsyncJobQueue}
 */
function getQueueFor(el) {
  if (!jobQueueMap.has(el)) {
    jobQueueMap.set(el, new AsyncJobQueue());
  }
  return jobQueueMap.get(el);
}

/**
 * Plays a sound.
 * @param {!HTMLAudioElement} el Audio element to play.
 * @return {!Promise} Promise which will be resolved once the sound is ended or
 *     paused.
 */
export function play(el) {
  cancel(el);
  const queue = getQueueFor(el);
  const job = async () => {
    el.currentTime = 0;
    await el.play();

    const audioEnded = new WaitableEvent();
    const events = ['ended', 'pause'];
    const onAudioStopped = () => {
      audioEnded.signal();
      for (const event of events) {
        el.removeEventListener(event, onAudioStopped);
      }
    };
    for (const event of events) {
      el.addEventListener(event, onAudioStopped);
    }
    return audioEnded.wait();
  };
  return queue.push(job);
}

/**
 * Cancel a sound from playing.
 * @param {!HTMLAudioElement} el Audio element to cancel.
 * @return {!Promise}
 */
export async function cancel(el) {
  el.pause();
  await getQueueFor(el).flush();
}
