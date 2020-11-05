// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {Intent} from '../intent.js';  // eslint-disable-line no-unused-vars
import * as Comlink from '../lib/comlink.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessorHelperInterface} from '../untrusted_helper_interfaces.js';
import * as util from '../util.js';

import {AsyncWriter} from './async_writer.js';
import {createPrivateTempVideoFile} from './file_system.js';
// eslint-disable-next-line no-unused-vars
import {AbstractFileEntry} from './file_system_entry.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessor} from './video_processor_interface.js';

const Mp4VideoProcessor = (async () => {
  const workerChannel = new MessageChannel();
  const videoProcessorHelper = /** @type {!VideoProcessorHelperInterface} */ (
      await util.createUntrustedJSModule(
          '/js/untrusted_video_processor_helper.js',
          browserProxy.getUntrustedOrigin()));
  await videoProcessorHelper.connectToWorker(
      Comlink.transfer(workerChannel.port2, [workerChannel.port2]));
  return Comlink.wrap(workerChannel.port1);
})();

/**
 * @param {!AsyncWriter} output
 * @return {!Promise<!VideoProcessor>}
 */
async function createVideoProcessor(output) {
  // Comlink proxies all calls asynchronously, including constructors.
  return new (await Mp4VideoProcessor)(
      Comlink.proxy(output), {seekable: output.seekable()});
}

/**
 * @param {!Intent} intent
 * @return {!AsyncWriter}
 */
function createWriterForIntent(intent) {
  const write = async (blob) => {
    await intent.appendData(new Uint8Array(await blob.arrayBuffer()));
  };
  // TODO(crbug.com/1140852): Supports seek.
  return new AsyncWriter({write, seek: null, close: null});
}

/**
 * Used to save captured video.
 */
export class VideoSaver {
  /**
   * @param {!AbstractFileEntry} file
   * @param {!VideoProcessor} processor
   */
  constructor(file, processor) {
    /**
     * @const {!AbstractFileEntry}
     */
    this.file_ = file;

    /**
     * @const {!VideoProcessor}
     */
    this.processor_ = processor;
  }

  /**
   * Writes video data to result video.
   * @param {!Blob} blob Video data to be written.
   */
  write(blob) {
    this.processor_.write(blob);
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   * @return {!Promise<!AbstractFileEntry>} Result video file.
   */
  async endWrite() {
    await this.processor_.close();
    return this.file_;
  }

  /**
   * Creates video saver for the given file.
   * @param {!AbstractFileEntry} file
   * @return {!Promise<!VideoSaver>}
   */
  static async createForFile(file) {
    const writer = await file.getWriter();
    const processor = await createVideoProcessor(writer);
    return new VideoSaver(file, processor);
  }

  /**
   * Creates video saver for the given intent.
   * @param {!Intent} intent
   * @return {!Promise<!VideoSaver>}
   */
  static async createForIntent(intent) {
    const file = await createPrivateTempVideoFile();
    const fileWriter = await file.getWriter();
    const intentWriter = createWriterForIntent(intent);
    const writer = AsyncWriter.combine(fileWriter, intentWriter);
    const processor = await createVideoProcessor(writer);
    return new VideoSaver(file, processor);
  }
}
