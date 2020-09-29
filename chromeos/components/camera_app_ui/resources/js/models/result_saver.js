// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {VideoSaver} from './video_saver.js';

/**
 * Handles captured result photos and video.
 * @interface
 */
export class ResultSaver {
  /**
   * Saves photo capture result.
   * @param {!Blob} blob Data of the photo to be added.
   * @param {string} name Name of the photo to be saved.
   * @return {!Promise} Promise for the operation.
   */
  async savePhoto(blob, name) {}

  /**
   * Returns a video saver to save captured result video.
   * @return {!Promise<!VideoSaver>}
   */
  async startSaveVideo() {}

  /**
   * Saves captured video result.
   * @param {!VideoSaver} video Contains the video result to be
   *     saved.
   * @return {!Promise}
   */
  async finishSaveVideo(video) {}
}
