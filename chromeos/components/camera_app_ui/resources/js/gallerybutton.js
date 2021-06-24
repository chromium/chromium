// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from './chrome_util.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import * as filesystem from './models/file_system.js';
import {
  DirectoryAccessEntry,  // eslint-disable-line no-unused-vars
  FileAccessEntry,       // eslint-disable-line no-unused-vars
} from './models/file_system_access_entry.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from './models/result_saver.js';
import {VideoSaver} from './models/video_saver.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {scaleImage, scaleVideo} from './thumbnailer.js';
import {
  ErrorLevel,
  ErrorType,
} from './type.js';

/**
 * Width of thumbnail used by cover photo of gallery button.
 * @type {number}
 */
const THUMBNAIL_WIDTH = 240;

/**
 * Cover photo of gallery button.
 */
class CoverPhoto {
  /**
   * @param {!FileAccessEntry} file File entry of cover photo.
   * @param {?string} thumbnailUrl Url to its thumbnail. Might be null if the
   *     thumbnail is failed to load.
   */
  constructor(file, thumbnailUrl) {
    /**
     * @type {!FileAccessEntry}
     * @const
     */
    this.file = file;

    /**
     * @type {?string}
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
    if (this.thumbnailUrl !== null) {
      URL.revokeObjectURL(this.thumbnailUrl);
    }
  }

  /**
   * Creates CoverPhoto objects from photo file.
   * @param {!FileAccessEntry} file
   * @return {!Promise<?CoverPhoto>}
   */
  static async create(file) {
    const blob = await file.file();
    if (blob.size === 0) {
      reportError(
          ErrorType.EMPTY_FILE,
          ErrorLevel.ERROR,
          new Error('The file to generate cover photo is empty'),
      );
      return null;
    }

    try {
      const thumbnail = filesystem.hasVideoPrefix(file) ?
          await scaleVideo(blob, THUMBNAIL_WIDTH) :
          await scaleImage(blob, THUMBNAIL_WIDTH);
      return new CoverPhoto(file, URL.createObjectURL(thumbnail));
    } catch (e) {
      reportError(
          ErrorType.BROKEN_THUMBNAIL, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
      return new CoverPhoto(file, null);
    }
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
     * @type {?DirectoryAccessEntry}
     * @private
     */
    this.directory_ = null;

    this.button_.addEventListener('click', async () => {
      if (this.cover_ !== null) {
        await ChromeHelper.getInstance().openFileInGallery(
            this.cover_.file.name);
      }
    });
  }

  /**
   * Initializes the gallery button.
   * @param {!DirectoryAccessEntry} dir Directory holding saved pictures
   *     showing in gallery.
   */
  async initialize(dir) {
    this.directory_ = dir;
    await this.checkCover_();
  }

  /**
   * @param {?FileAccessEntry} file File to be set as cover photo.
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
        cover !== null && cover.thumbnailUrl !== null ?
        `url("${cover.thumbnailUrl}")` :
        'none';

    if (cover !== null) {
      ChromeHelper.getInstance().monitorFileDeletion(file.name, () => {
        this.checkCover_();
      });
    }
  }

  /**
   * Checks validity of cover photo from camera directory.
   * @private
   */
  async checkCover_() {
    if (this.directory_ === null) {
      return;
    }
    const dir = this.directory_;

    // Checks existence of cached cover photo.
    if (this.cover_ !== null) {
      if (await dir.isExist(this.cover_.name)) {
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
    const file = await filesystem.saveBlob(blob, name);

    ChromeHelper.getInstance().sendNewCaptureBroadcast(
        {isVideo: false, name: file.name});
    await this.updateCover_(file);
  }

  /**
   * @override
   */
  async startSaveVideo(videoRotation) {
    const file = await filesystem.createVideoFile();
    return VideoSaver.createForFile(file, videoRotation);
  }

  /**
   * @override
   */
  async finishSaveVideo(video) {
    const file = await video.endWrite();
    assert(file !== null);

    ChromeHelper.getInstance().sendNewCaptureBroadcast(
        {isVideo: true, name: file.name});
    await this.updateCover_(file);
  }
}
