// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {Intent} from '../intent.js';  // eslint-disable-line no-unused-vars
import * as Comlink from '../lib/comlink.js';
import {AsyncWriter} from './async_writer.js';
import {createPrivateTempVideoFile} from './file_system.js';
// eslint-disable-next-line no-unused-vars
import {AbstractFileEntry} from './file_system_entry.js';

const VideoProcessor = (() => {
  const url = browserProxy.isMp4RecordingEnabled() ?
      '/js/models/mp4_video_processor.js' :
      '/js/models/nop_video_processor.js';
  return Comlink.wrap(new Worker(url, {type: 'module'}));
})();

/**
 * @param {!AsyncWriter} output
 * @return {!Promise<!AsyncWriter>}
 */
async function createVideoProcesoor(output) {
  // Comlink proxies all calls asynchronously, including constructors.
  return await new VideoProcessor(Comlink.proxy(output));
}

/**
 * @param {!Intent} intent
 * @return {!AsyncWriter}
 */
function createWriterForIntent(intent) {
  const doWrite = async (blob) => {
    await intent.appendData(new Uint8Array(await blob.arrayBuffer()));
  };
  return new AsyncWriter(doWrite);
}

/**
 * Used to save captured video.
 */
export class VideoSaver {
  /**
   * @param {!AbstractFileEntry} file
   * @param {!AsyncWriter} processor
   */
  constructor(file, processor) {
    /**
     * @const {!AbstractFileEntry}
     */
    this.file_ = file;

    /**
     * @const {!AsyncWriter}
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
    const processor = await createVideoProcesoor(writer);
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
    const processor = await createVideoProcesoor(writer);
    return new VideoSaver(file, processor);
  }
}
