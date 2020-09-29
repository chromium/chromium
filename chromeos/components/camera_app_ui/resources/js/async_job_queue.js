// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asynchronous job queue.
 */
export class AsyncJobQueue {
  /**
   * @public
   */
  constructor() {
    this.promise_ = Promise.resolve();
  }

  /**
   * Pushes the given job into queue.
   * @param {function(): !Promise} job
   * @return {!Promise} Resolved when the job is finished.
   */
  push(job) {
    this.promise_ = this.promise_.then(() => job());
    return this.promise_;
  }

  /**
   * Flushes the job queue.
   * @return {!Promise} Resolved when all jobs in the queue are finished.
   */
  async flush() {
    await this.promise_;
  }
}
