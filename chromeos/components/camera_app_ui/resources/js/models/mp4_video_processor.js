// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from '../async_job_queue.js';
import {assert, assertNotReached} from '../chrome_util.js';
import * as Comlink from '../lib/comlink.js';
import runFFmpeg from '../lib/ffmpeg.js';
import {WaitableEvent} from '../waitable_event.js';
// eslint-disable-next-line no-unused-vars
import {AsyncWriter} from './async_writer.js';

/**
 * A file stream in Emscripten.
 * @typedef {{position: number}}
 */
let FileStream;  // eslint-disable-line no-unused-vars

/**
 * The set of callbacks for an emulated device in Emscripten. ref:
 * https://emscripten.org/docs/api_reference/Filesystem-API.html#FS.registerDevice
 * @typedef {{
 *   open: function(!FileStream): undefined,
 *   close: function(!FileStream): undefined,
 *   read: function(!FileStream, !Int8Array, number, number, number): number,
 *   write: function(!FileStream, !Int8Array, number, number, number=): number,
 *   llseek: function(!FileStream, number, number): number,
 * }}
 */
let FileOps;  // eslint-disable-line no-unused-vars

/**
 * An emulated stdin device backed by Int8Array.
 */
class StdinDevice {
  /**
   * @public
   */
  constructor() {
    /**
     * The data to be read from the device.
     * @type {!Array<!Int8Array>}
     */
    this.data_ = [];

    /**
     * Whether the writing is ended. If true, no more data would be pushed.
     * @type {boolean}
     */
    this.ended_ = false;

    /**
     * The callback to be triggered when the device is ready to read(). The
     * callback would be called only once and reset to null afterward.
     * @type {?function(): void}
     */
    this.readableCallback_ = null;
  }

  /**
   * Notifies and resets the readable callback, if any.
   */
  consumeReadableCallback() {
    if (this.readableCallback_ === null) {
      return;
    }
    const callback = this.readableCallback_;
    this.readableCallback_ = null;
    callback();
  }

  /**
   * Pushes a chunk of data into the device.
   * @param {!Int8Array} data
   */
  push(data) {
    assert(!this.ended_);
    this.data_.push(data);
    this.consumeReadableCallback();
  }

  /**
   * Closes the writing pipe.
   */
  endPush() {
    this.ended_ = true;
    this.consumeReadableCallback();
  }

  /**
   * Implements the read() operation for the emulated device.
   * @param {!FileStream} stream
   * @param {!Int8Array} buffer The destination buffer.
   * @param {number} offset The destination buffer offset.
   * @param {number} length The maximum length to read.
   * @param {number} position The position to read from stream.
   * @return {number} The numbers of bytes read.
   */
  read(stream, buffer, offset, length, position) {
    assert(position === stream.position, 'stdin is not seekable');
    if (this.data_.length === 0) {
      assert(this.ended_);
      return 0;
    }

    let bytesRead = 0;
    while (this.data_.length > 0 && length > 0) {
      const data = this.data_[0];
      const len = Math.min(data.length, length);
      buffer.set(data.subarray(0, len), offset);
      if (len === data.length) {
        this.data_.shift();
      } else {
        this.data_[0] = data.subarray(len);
      }

      offset += len;
      length -= len;
      bytesRead += len;
    }

    return bytesRead;
  }

  /**
   * @return {!FileOps}
   */
  getFileOps() {
    return {
      open: () => {},
      close: () => {},
      read: this.read.bind(this),
      write: () => assertNotReached('write should not be called on stdin'),
      llseek: () => assertNotReached('llseek should not be called on stdin'),
    };
  }

  /**
   * Sets the readable callback. The callback would be called immediately if
   * the device is in a readable state.
   * @param {function(): undefined} callback
   */
  setReadableCallback(callback) {
    if (this.data_.length > 0 || this.ended_) {
      callback();
      return;
    }
    assert(this.readableCallback_ === null);
    this.readableCallback_ = callback;
  }
}

/**
 * An emulated stdout device.
 */
class StdoutDevice {
  /**
   * @param {!AsyncWriter} output Where should the device write to.
   */
  constructor(output) {
    /**
     * @type {!AsyncWriter}
     * @private
     */
    this.output_ = output;

    /**
     * @type {!WaitableEvent}
     * @private
     */
    this.closed_ = new WaitableEvent();
  }

  /**
   * Implements the write() operation for the emulated device.
   * @param {!FileStream} stream
   * @param {!Int8Array} buffer The source buffer.
   * @param {number} offset The source buffer offset.
   * @param {number} length The maximum length to be write.
   * @param {number=} position The position to write in stream.
   * @return {number} The numbers of bytes written.
   */
  write(stream, buffer, offset, length, position) {
    assert(!this.closed_.isSignaled());
    const blob = new Blob([buffer.subarray(offset, offset + length)]);
    assert(
        position === undefined || position === stream.position,
        'stdout is not seekable');
    this.output_.write(blob);
    return length;
  }

  /**
   * Implements the close() operation for the emulated device.
   */
  close() {
    this.closed_.signal();
  }

  /**
   * @return {!Promise} Resolved when the device is closed.
   */
  async waitClosed() {
    await this.closed_.wait();
  }

  /**
   * @return {!FileOps}
   */
  getFileOps() {
    return {
      open: () => {},
      close: this.close.bind(this),
      read: () => assertNotReached('read should not be called on stdout'),
      write: this.write.bind(this),
      llseek: () => assertNotReached('llseek should not be called on stdout'),
    };
  }
}

/**
 * A video processor that can remux mkv to mp4.
 */
class Mp4VideoProcessor {
  /**
   * @param {!AsyncWriter} output The output writer of mp4.
   */
  constructor(output) {
    this.output_ = output;
    this.stdin_ = new StdinDevice();
    this.stdout_ = new StdoutDevice(output);
    this.jobQueue_ = new AsyncJobQueue();

    const config = {
      arguments: [
        // Make the procssing pipeline start earlier by shorten the initial
        // analyze durtaion from the default 5s to 1s. This reduce the
        // stop-capture lantency significantly for short videos.
        '-analyzeduration', '1M',
        // mkv input from stdin
        '-f', 'matroska', '-i', 'pipe:0',
        // transcode audio to aac and copy the video
        '-c:a', 'aac', '-c:v', 'copy',
        // verbose and don't ask anything
        '-hide_banner', '-loglevel', 'error', '-nostdin', '-y',
        // streaming friendly output
        '-movflags', 'frag_keyframe', '-frag_duration', '100000',
        // mp4 output to stdout
        '-f', 'mp4', 'pipe:1'  // eslint-disable-line comma-dangle
      ],
      locateFile: (file) => {
        assert(file === 'ffmpeg.wasm');
        return '/js/lib/ffmpeg.wasm';
      },
      noFSInit: true,  // It would be setup in preRun().
      preRun: () => {
        /** @suppress {missingProperties} */
        const fs = config.FS;
        assert(fs !== null);
        // 80 is just a random major number that won't collide with other
        // default devices of the Emscripten runtime environment, which uses
        // major numbers 1, 3, 5, 6, 64, and 65. Ref:
        // https://github.com/emscripten-core/emscripten/blob/1ed6dd5cfb88d927ec03ecac8756f0273810d5c9/src/library_fs.js#L1331
        const input = fs.makedev(80, 0);
        fs.registerDevice(input, this.stdin_.getFileOps());
        fs.mkdev('/dev/stdin', input);
        const output = fs.makedev(80, 1);
        fs.registerDevice(output, this.stdout_.getFileOps());
        fs.mkdev('/dev/stdout', output);
        fs.symlink('/dev/tty1', '/dev/stderr');
        const stdin = fs.open('/dev/stdin', 'r');
        const stdout = fs.open('/dev/stdout', 'w');
        const stderr = fs.open('/dev/stderr', 'w');
        assert(stdin.fd === 0);
        assert(stdout.fd === 1);
        assert(stderr.fd === 2);
      },
    };

    const initFFmpeg = () => {
      return new Promise((resolve) => {
        // runFFmpeg() is a special function exposed by Emscripten that will
        // return an object with then(). The function passed into then() would
        // be called when the runtime is initialized. Note that because the
        // then() function will return the object itself again, using await here
        // would cause an infinite loop.
        runFFmpeg(config).then(() => resolve());
      });
    };
    this.jobQueue_.push(initFFmpeg);

    // This is a function to be called by ffmpeg before running read() in C.
    globalThis.waitReadable = (callback) => {
      this.stdin_.setReadableCallback(callback);
    };
  }

  /**
   * Writes a blob with mkv data into the processor.
   * @param {!Blob} blob
   */
  write(blob) {
    this.jobQueue_.push(async () => {
      const buf = await blob.arrayBuffer();
      this.stdin_.push(new Int8Array(buf));
    });
  }

  /**
   * Closes the writer. No more write operations are allowed.
   * @return {!Promise} Resolved when all write operations are finished.
   */
  async close() {
    // Flush and close stdin.
    this.jobQueue_.push(async () => {
      this.stdin_.endPush();
    });
    await this.jobQueue_.flush();

    // Wait until stdout is closed.
    await this.stdout_.waitClosed();

    // Flush and close the output writer.
    await this.output_.close();
  }
}

Comlink.expose(Mp4VideoProcessor);
