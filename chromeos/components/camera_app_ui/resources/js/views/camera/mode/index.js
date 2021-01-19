// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from '../../../chrome_util.js';
import {
  CaptureCandidate,           // eslint-disable-line no-unused-vars
  ConstraintsPreferrer,       // eslint-disable-line no-unused-vars
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../../../device/constraints_preferrer.js';
import * as dom from '../../../dom.js';
// eslint-disable-next-line no-unused-vars
import {DeviceOperator} from '../../../mojo/device_operator.js';
import * as state from '../../../state.js';
import {
  Facing,  // eslint-disable-line no-unused-vars
  Mode,
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';
import * as util from '../../../util.js';

import {
  ModeBase,     // eslint-disable-line no-unused-vars
  ModeFactory,  // eslint-disable-line no-unused-vars
} from './mode_base.js';
import {
  PhotoFactory,
  PhotoHandler,
  PhotoResult,
} from './photo.js';
import {PortraitFactory} from './portrait.js';
import {SquareFactory} from './square.js';
import {
  Video,
  VideoFactory,
  VideoHandler,
  VideoResult,
} from './video.js';

export {PhotoHandler, PhotoResult, Video, VideoHandler, VideoResult};

/**
 * Callback to trigger mode switching.
 * return {!Promise}
 * @typedef {function(): !Promise}
 */
export let DoSwitchMode;

/* eslint-disable no-unused-vars */

/**
 * The abstract interface for the mode configuration.
 * @interface
 */
class ModeConfig {
  /**
   * @param {?string} deviceId
   * @return {!Promise<boolean>} Resolves to boolean indicating whether the mode
   *     is supported by video device with specified device id.
   * @abstract
   */
  async isSupported(deviceId) {}

  /**
   * Get stream constraints for HALv1 of this mode.
   * @param {?string} deviceId
   * @return {!Array<!MediaStreamConstraints>}
   * @abstract
   */
  getV1Constraints(deviceId) {}

  /* eslint-disable getter-return */

  /**
   * Gets factory to create capture object for this mode.
   * @return {!ModeFactory}
   * @abstract
   */
  get captureFactory() {}

  /**
   * HALv3 constraints preferrer for this mode.
   * @return {!ConstraintsPreferrer}
   * @abstract
   */
  get constraintsPreferrer() {}

  /**
   * Mode to be fallbacked to when fail to configure this mode.
   * @return {!Mode}
   * @abstract
   */
  get nextMode() {}

  /* eslint-enable getter-return */
}

/* eslint-enable no-unused-vars */

/**
 * Mode controller managing capture sequence of different camera mode.
 */
export class Modes {
  /**
   * @param {!Mode} defaultMode Default mode to be switched to.
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @param {!DoSwitchMode} doSwitchMode
   * @param {!PhotoHandler} photoHandler
   * @param {!VideoHandler} videoHandler
   */
  constructor(
      defaultMode, photoPreferrer, videoPreferrer, doSwitchMode, photoHandler,
      videoHandler) {
    /**
     * @type {!DoSwitchMode}
     * @private
     */
    this.doSwitchMode_ = doSwitchMode;

    /**
     * Capture controller of current camera mode.
     * @type {?ModeBase}
     */
    this.current = null;

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.modesGroup_ = dom.get('#modes-group', HTMLElement);

    /**
     * Returns a set of available constraints for HALv1 device.
     * @param {boolean} videoMode Is getting constraints for video mode.
     * @param {?string} deviceId Id of video device.
     * @return {!Array<!MediaStreamConstraints>} Result of
     *     constraints-candidates.
     */
    const getV1Constraints = function(videoMode, deviceId) {
      const /** !Array<!MediaTrackConstraints> */ baseConstraints = [
        {
          aspectRatio: {ideal: videoMode ? 1.7777777778 : 1.3333333333},
          width: {min: 1280},
          frameRate: {min: 20, ideal: 30},
        },
        {
          width: {min: 640},
          frameRate: {min: 20, ideal: 30},
        },
      ];
      return baseConstraints.map((constraint) => {
        if (deviceId) {
          constraint.deviceId = {exact: deviceId};
        } else {
          // HALv1 devices are unable to know facing before stream
          // configuration, deviceId is set to null for requesting camera with
          // default facing.
          constraint.facingMode = {exact: util.getDefaultFacing()};
        }
        return {
          audio: videoMode ? {echoCancellation: false} : false,
          video: constraint,
        };
      });
    };

    /**
     * Mode classname and related functions and attributes.
     * @type {!Object<!Mode, !ModeConfig>}
     * @private
     */
    this.allModes_ = {
      [Mode.VIDEO]: {
        captureFactory: new VideoFactory(videoHandler),
        isSupported: async () => true,
        constraintsPreferrer: videoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, true),
        nextMode: Mode.PHOTO,
      },
      [Mode.PHOTO]: {
        captureFactory: new PhotoFactory(photoHandler),
        isSupported: async () => true,
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: Mode.SQUARE,
      },
      [Mode.SQUARE]: {
        captureFactory: new SquareFactory(photoHandler),
        isSupported: async () => true,
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: Mode.PHOTO,
      },
      [Mode.PORTRAIT]: {
        captureFactory: new PortraitFactory(photoHandler),
        isSupported: async (deviceId) => {
          if (deviceId === null) {
            return false;
          }
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return false;
          }
          return await deviceOperator.isPortraitModeSupported(deviceId);
        },
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: Mode.PHOTO,
      },
    };

    dom.getAll('.mode-item>input', HTMLInputElement).forEach((element) => {
      element.addEventListener('click', (event) => {
        if (!state.get(state.State.STREAMING) ||
            state.get(state.State.TAKING)) {
          event.preventDefault();
        }
      });
      element.addEventListener('change', async (event) => {
        if (element.checked) {
          const mode = /** @type {!Mode} */ (element.dataset['mode']);
          this.updateModeUI_(mode);
          state.set(state.State.MODE_SWITCHING, true);
          const isSuccess = await this.doSwitchMode_();
          state.set(state.State.MODE_SWITCHING, false, {hasError: !isSuccess});
        }
      });
    });

    [state.State.EXPERT, state.State.SAVE_METADATA].forEach(
        (/** !state.State */ s) => {
          state.addObserver(s, this.updateSaveMetadata_.bind(this));
        });

    // Set default mode when app started.
    this.updateModeUI_(defaultMode);
  }

  /**
   * @return {!Array<!Mode>}
   * @private
   */
  get allModeNames_() {
    return Object.keys(this.allModes_);
  }

  /**
   * Updates state of mode related UI to the target mode.
   * @param {!Mode} mode Mode to be toggled.
   * @private
   */
  updateModeUI_(mode) {
    this.allModeNames_.forEach((m) => state.set(m, m === mode));
    const element =
        dom.get(`.mode-item>input[data-mode=${mode}]`, HTMLInputElement);
    element.checked = true;
    const wrapper = assertInstanceof(element.parentElement, HTMLDivElement);
    const scrollLeft = wrapper.offsetLeft -
        (this.modesGroup_.offsetWidth - wrapper.offsetWidth) / 2;
    this.modesGroup_.scrollTo({
      left: scrollLeft,
      top: 0,
      behavior: 'smooth',
    });
  }

  /**
   * Gets all mode candidates. Desired trying sequence of candidate modes is
   * reflected in the order of the returned array.
   * @return {!Array<!Mode>} Mode candidates to be tried out.
   */
  getModeCandidates() {
    const tried = {};
    const /** !Array<!Mode> */ results = [];
    let mode = this.allModeNames_.find(state.get);
    assert(mode !== undefined);
    while (!tried[mode]) {
      tried[mode] = true;
      results.push(mode);
      mode = this.allModes_[mode].nextMode;
    }
    return results;
  }

  /**
   * Gets all available capture resolution and its corresponding preview
   * constraints for the given mode.
   * @param {!Mode} mode
   * @param {string} deviceId
   * @return {!Array<!CaptureCandidate>}
   */
  getResolutionCandidates(mode, deviceId) {
    return this.allModes_[mode].constraintsPreferrer.getSortedCandidates(
        deviceId);
  }

  /**
   * Gets capture resolution and its corresponding preview constraints for the
   * given mode on camera HALv1 device.
   * @param {!Mode} mode
   * @param {?string} deviceId
   * @return {!Array<!CaptureCandidate>}
   */
  getResolutionCandidatesV1(mode, deviceId) {
    const previewCandidates = this.allModes_[mode].getV1Constraints(deviceId);
    return [{resolution: null, previewCandidates}];
  }

  /**
   * Gets factory to create mode capture object.
   * @param {!Mode} mode
   * @return {!ModeFactory}
   */
  getModeFactory(mode) {
    return this.allModes_[mode].captureFactory;
  }

  /**
   * Gets supported modes for video device of given device id.
   * @param {?string} deviceId Device id of the video device.
   * @return {!Promise<!Array<!Mode>>} All supported mode for
   *     the video device.
   */
  async getSupportedModes(deviceId) {
    const /** !Array<!Mode> */ supportedModes = [];
    for (const mode of this.allModeNames_) {
      const obj = this.allModes_[mode];
      if (await obj.isSupported(deviceId)) {
        supportedModes.push(mode);
      }
    }
    return supportedModes;
  }

  /**
   * Updates mode selection UI according to given device id.
   * @param {?string} deviceId
   * @return {!Promise}
   */
  async updateModeSelectionUI(deviceId) {
    const supportedModes = await this.getSupportedModes(deviceId);
    const items = dom.getAll('div.mode-item', HTMLDivElement);
    let first = null;
    let last = null;
    items.forEach((el) => {
      const radio = dom.getFrom(el, 'input[type=radio]', HTMLInputElement);
      const supported =
          supportedModes.includes(/** @type {!Mode} */ (radio.dataset['mode']));
      el.classList.toggle('hide', !supported);
      if (supported) {
        if (first === null) {
          first = el;
        }
        last = el;
      }
    });
    items.forEach((el) => {
      el.classList.toggle('first', el === first);
      el.classList.toggle('last', el === last);
    });
  }

  /**
   * Creates and updates new current mode object.
   * @param {!Mode} mode Classname of mode to be updated.
   * @param {!ModeFactory} factory The factory ready for producing mode capture
   *     object.
   * @param {!MediaStream} stream Stream of the new switching mode.
   * @param {!Facing} facing Camera facing of the current mode.
   * @param {?string} deviceId Device id of currently working video device.
   * @param {?Resolution} captureResolution Capturing resolution width and
   *     height.
   * @return {!Promise}
   */
  async updateMode(mode, factory, stream, facing, deviceId, captureResolution) {
    if (this.current !== null) {
      await this.current.stopCapture();
    }
    this.updateModeUI_(mode);
    this.current = factory.produce();
    if (deviceId && captureResolution) {
      this.allModes_[mode].constraintsPreferrer.updateValues(
          deviceId, stream, facing, captureResolution);
    }
    await this.updateSaveMetadata_();
  }

  /**
   * Checks whether to save image metadata or not.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async updateSaveMetadata_() {
    if (state.get(state.State.EXPERT) && state.get(state.State.SAVE_METADATA)) {
      await this.enableSaveMetadata_();
    } else {
      await this.disableSaveMetadata_();
    }
  }

  /**
   * Enables save metadata of subsequent photos in the current mode.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async enableSaveMetadata_() {
    if (this.current !== null) {
      await this.current.addMetadataObserver();
    }
  }

  /**
   * Disables save metadata of subsequent photos in the current mode.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async disableSaveMetadata_() {
    if (this.current !== null) {
      await this.current.removeMetadataObserver();
    }
  }
}
