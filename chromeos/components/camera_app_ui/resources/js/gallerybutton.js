// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assert} from './chrome_util.js';
import * as dom from './dom.js';
import * as filesystem from './models/file_system.js';
import {
  AbstractDirectoryEntry,  // eslint-disable-line no-unused-vars
  AbstractFileEntry,       // eslint-disable-line no-unused-vars
} from './models/file_system_entry.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from './models/result_saver.js';
import {VideoSaver} from './models/video_saver.js';
import * as util from './util.js';

/**
 * Width of thumbnail used by cover photo of gallery button.
 * @type {number}
 */
const THUMBNAIL_WIDTH = 240;

/**
 * The maximum size of video used to generate the thumbnail. The file would be
 * truncated to that size before generating the thumbnail, which speeds up the
 * generation process significantly when the file is large. 32MB should be more
 * than enough for getting the first frame from a video.
 * @type {number}
 */
const VIDEO_THUMBNAIL_SIZE_LIMIT = 32 << 20;

/**
 * Cover photo of gallery button.
 */
class CoverPhoto {
  /**
   * @param {!AbstractFileEntry} file File entry of cover photo.
   * @param {string} thumbnailUrl Url to its thumbnail.
   */
  constructor(file, thumbnailUrl) {
    /**
     * @type {!AbstractFileEntry}
     * @const
     */
    this.file = file;

    /**
     * @type {string}
     * @const
     */
    this.thumbnailUrl = thumbnailUrl;
  }

  /**
   * File name of the cover photo.
   * @return {string}
   */
  get name() {
    return this.file.name;
  }

  /**
   * Releases resources used by this cover photo.
   */
  release() {
    URL.revokeObjectURL(this.thumbnailUrl);
  }

  /**
   * Creates CoverPhoto objects from photo file.
   * @param {!AbstractFileEntry} file
   * @return {!Promise<!CoverPhoto>}
   */
  static async create(file) {
    const isVideo = filesystem.hasVideoPrefix(file);
    const limit = isVideo ? VIDEO_THUMBNAIL_SIZE_LIMIT : Infinity;
    const fileUrl = await filesystem.pictureURL(file, {limit});
    const thumbnail =
        await util.scalePicture(fileUrl, isVideo, THUMBNAIL_WIDTH);
    URL.revokeObjectURL(fileUrl);

    return new CoverPhoto(file, URL.createObjectURL(thumbnail));
  }
}

/**
 * Creates a controller for the gallery-button.
 * @implements {ResultSaver}
 */
export class GalleryButton {
  /**
   * @public
   */
  constructor() {
    /**
     * Cover photo from latest saved picture.
     * @type {?CoverPhoto}
     * @private
     */
    this.cover_ = null;

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.button_ = dom.get('#gallery-enter', HTMLButtonElement);

    /**
     * Directory holding saved pictures showing in gallery.
     * @type {?AbstractDirectoryEntry}
     * @private
     */
    this.directory_ = null;

    this.button_.addEventListener('click', async () => {
      // Check if the last picture serving as cover photo still exists before
      // opening it in the gallery.
      // TODO(yuli): Remove this workaround for unable watching changed-files.
      await this.checkCover_();
      if (this.cover_ !== null) {
        await browserProxy.openGallery(this.cover_.file);
      }
    });
  }

  /**
   * Initializes the gallery button.
   * @param {!AbstractDirectoryEntry} dir Directory holding saved pictures
   *     showing in gallery.
   */
  async initialize(dir) {
    this.directory_ = dir;
    await this.checkCover_();
  }

  /**
   * @param {?AbstractFileEntry} file File to be set as cover photo.
   * @return {!Promise}
   * @private
   */
  async updateCover_(file) {
    const cover = file === null ? null : await CoverPhoto.create(file);
    if (this.cover_ === cover) {
      return;
    }
    if (this.cover_ !== null) {
      this.cover_.release();
    }
    this.cover_ = cover;

    this.button_.hidden = cover === null;
    this.button_.style.backgroundImage =
        cover !== null ? `url("${cover.thumbnailUrl}")` : 'none';
  }

  /**
   * Checks validity of cover photo from download directory.
   * @private
   */
  async checkCover_() {
    if (this.directory_ === null) {
      return;
    }
    const dir = this.directory_;

    // Checks existence of cached cover photo.
    if (this.cover_ !== null) {
      const file = await dir.getFile(this.cover_.name);
      if (file !== null) {
        return;
      }
    }

    // Rescan file system.
    const files = await filesystem.getEntries();
    if (files.length === 0) {
      await this.updateCover_(null);
      return;
    }
    const filesWithTime = await Promise.all(
        files.map(async (file) => ({
                    file,
                    time: (await file.getLastModificationTime()),
                  })));
    const lastFile =
        filesWithTime.reduce((last, cur) => last.time > cur.time ? last : cur)
            .file;
    await this.updateCover_(lastFile);
  }

  /**
   * @override
   */
  async savePhoto(blob, name) {
    const orientedPhoto = await new Promise((resolve) => {
      // Ignore errors since it is better to save something than
      // nothing.
      // TODO(yuli): Support showing images by EXIF orientation
      // instead.
      util.orientPhoto(blob, resolve, () => resolve(blob));
    });
    const file = await filesystem.saveBlob(orientedPhoto, name);
    assert(file !== null);
    await this.updateCover_(file);
  }

  /**
   * @override
   */
  async startSaveVideo() {
    const file = await filesystem.createVideoFile();
    return VideoSaver.createForFile(file);
  }

  /**
   * @override
   */
  async finishSaveVideo(video) {
    const file = await video.endWrite();
    assert(file !== null);
    await this.updateCover_(file);
  }
}
