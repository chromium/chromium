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
   * @param {number} videoRotation Clock-wise rotation in degrees to set in the
   *     video metadata so that the saved video can be displayed in upright
   *     orientation.
   * @return {!Promise<!VideoSaver>}
   */
  async startSaveVideo(videoRotation) {}

  /**
   * Returns a gif saver to save captured result gif.
   * @param {number} width
   * @param {number} height
   * @return {!Promise<!VideoSaver>}
   */
  async startSaveGIF(width, height) {}

  /**
   * Saves captured video result.
   * @param {!VideoSaver} video Contains the video result to be
   *     saved.
   * @return {!Promise}
   */
  async finishSaveVideo(video) {}

  /**
   * Saves captured gif result.
   * @param {!VideoSaver} gifVideo Contains the gif result to be
   *     saved.
   * @return {!Promise}
   */
  async finishSaveGIF(gifVideo) {}
}
