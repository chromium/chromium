// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Filenamer} from '../../../models/file_namer.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import {PerfEvent} from '../../../perf.js';
import * as state from '../../../state.js';
import * as toast from '../../../toast.js';
import {
  Facing,      // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';
import * as util from '../../../util.js';

import {
  Photo,
  PhotoHandler,  // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Portrait mode capture controller.
 */
export class Portrait extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution
   * @param {!PhotoHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution, handler);
  }

  /**
   * @override
   */
  async start_() {
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new CrosImageCapture(this.stream_.getVideoTracks()[0]);
    }

    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    const filenamer = new Filenamer();
    const refImageName = filenamer.newBurstName(false);
    const portraitImageName = filenamer.newBurstName(true);

    if (this.metadataObserverId_ !== null) {
      [refImageName, portraitImageName].forEach((/** string */ imageName) => {
        this.metadataNames_.push(Filenamer.getMetadataName(imageName));
      });
    }

    let /** ?Promise<!Blob> */ reference;
    let /** ?Promise<!Blob> */ portrait;

    try {
      [reference, portrait] = await this.crosImageCapture_.takePhoto(
          photoSettings, [cros.mojom.Effect.PORTRAIT_MODE]);
      this.handler_.playShutterEffect();
    } catch (e) {
      toast.show('error_msg_take_photo_failed');
      throw e;
    }

    state.set(PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, true);
    let hasError = false;

    /**
     * @param {!Promise<!Blob>} p
     * @param {string} imageName
     * @return {!Promise}
     */
    const saveResult = async (p, imageName) => {
      const isPortrait = Object.is(p, portrait);
      let /** ?Blob */ blob = null;
      try {
        blob = await p;
      } catch (e) {
        hasError = true;
        toast.show(
            isPortrait ? 'error_msg_take_portrait_photo_failed' :
                         'error_msg_take_photo_failed');
        throw e;
      }
      const {width, height} = await util.blobToImage(blob);
      await this.handler_.handleResultPhoto(
          {resolution: new Resolution(width, height), blob}, imageName);
    };

    const refSave = saveResult(reference, refImageName);
    const portraitSave = saveResult(portrait, portraitImageName);
    try {
      await portraitSave;
    } catch (e) {
      hasError = true;
      // Portrait image may failed due to absence of human faces.
      // TODO(inker): Log non-intended error.
    }
    await refSave;
    state.set(
        PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, false,
        {hasError, facing: this.facing_});
  }
}
