// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {
  assert,
  assertInstanceof,
} from '../chrome_util.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../device/constraints_preferrer.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
import * as dom from '../dom.js';
import * as metrics from '../metrics.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from '../models/result_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as nav from '../nav.js';
// eslint-disable-next-line no-unused-vars
import {PerfLogger} from '../perf.js';
import * as sound from '../sound.js';
import * as state from '../state.js';
import * as toast from '../toast.js';
import {
  Facing,
  Mode,
  ViewName,
} from '../type.js';
import * as util from '../util.js';

import {Layout} from './camera/layout.js';
import {   // eslint-disable-line no-unused-vars
  Modes,
  PhotoHandler,  // eslint-disable-line no-unused-vars
  Video,
  VideoHandler,  // eslint-disable-line no-unused-vars
} from './camera/mode/index.js';
import {Options} from './camera/options.js';
import {Preview} from './camera/preview.js';
import * as timertick from './camera/timertick.js';
import {View} from './view.js';

/**
 * Thrown when app window suspended during stream reconfiguration.
 */
class CameraSuspendedError extends Error {
  /**
   * @param {string=} message Error message.
   */
  constructor(message = 'Camera suspended.') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Camera-view controller.
 * @implements {VideoHandler}
 * @implements {PhotoHandler}
 */
export class Camera extends View {
  /**
   * @param {!ResultSaver} resultSaver
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @param {!Mode} defaultMode
   * @param {!PerfLogger} perfLogger
   */
  constructor(
      resultSaver, infoUpdater, photoPreferrer, videoPreferrer, defaultMode,
      perfLogger) {
    super(ViewName.CAMERA);

    /**
     * @type {!DeviceInfoUpdater}
     * @private
     */
    this.infoUpdater_ = infoUpdater;

    /**
     * @type {!Mode}
     * @protected
     */
    this.defaultMode_ = defaultMode;

    /**
     * @type {!PerfLogger}
     * @private
     */
    this.perfLogger_ = perfLogger;

    /**
     * Layout handler for the camera view.
     * @type {!Layout}
     * @private
     */
    this.layout_ = new Layout();

    /**
     * Video preview for the camera.
     * @type {!Preview}
     * @private
     */
    this.preview_ = new Preview(this.start.bind(this));

    /**
     * Options for the camera.
     * @type {!Options}
     * @private
     */
    this.options_ = new Options(infoUpdater, this.start.bind(this));

    /**
     * @type {!ResultSaver}
     * @protected
     */
    this.resultSaver_ = resultSaver;

    /**
     * Device id of video device of active preview stream. Sets to null when
     * preview become inactive.
     * @type {?string}
     * @private
     */
    this.activeDeviceId_ = null;

    /**
     * Modes for the camera.
     * @type {!Modes}
     * @private
     */
    this.modes_ = new Modes(
        this.defaultMode_, photoPreferrer, videoPreferrer,
        this.start.bind(this), this, this);

    /**
     * @type {!Facing}
     * @protected
     */
    this.facingMode_ = Facing.UNKNOWN;

    /**
     * @type {!metrics.ShutterType}
     * @protected
     */
    this.shutterType_ = metrics.ShutterType.UNKNOWN;

    /**
     * @type {boolean}
     * @private
     */
    this.locked_ = false;

    /**
     * @type {?number}
     * @private
     */
    this.retryStartTimeout_ = null;

    /**
     * Promise for the camera stream configuration process. It's resolved to
     * boolean for whether the configuration is failed and kick out another
     * round of reconfiguration. Sets to null once the configuration is
     * completed.
     * @type {?Promise<boolean>}
     * @private
     */
    this.configuring_ = null;

    /**
     * Promise for the current take of photo or recording.
     * @type {?Promise}
     * @protected
     */
    this.take_ = null;

    /**
     * Gets type of ways to trigger shutter from click event.
     * @param {!MouseEvent} e
     * @return {!metrics.ShutterType}
     */
    const getShutterType = (e) => {
      if (e.clientX === 0 && e.clientY === 0) {
        return metrics.ShutterType.KEYBOARD;
      }
      return e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents ?
          metrics.ShutterType.TOUCH :
          metrics.ShutterType.MOUSE;
    };

    document.querySelector('#start-takephoto')
        .addEventListener('click', (e) => {
          const mouseEvent = assertInstanceof(e, MouseEvent);
          this.beginTake_(getShutterType(mouseEvent));
        });

    document.querySelector('#stop-takephoto')
        .addEventListener('click', () => this.endTake_());

    const videoShutter = document.querySelector('#recordvideo');
    videoShutter.addEventListener('click', (e) => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake_(getShutterType(assertInstanceof(e, MouseEvent)));
      } else {
        this.endTake_();
      }
    });

    document.querySelector('#video-snapshot').addEventListener('click', () => {
      const videoMode = assertInstanceof(this.modes_.current, Video);
      videoMode.takeSnapshot();
    });

    const pauseShutter = document.querySelector('#pause-recordvideo');
    pauseShutter.addEventListener('click', () => {
      const videoMode = assertInstanceof(this.modes_.current, Video);
      videoMode.togglePaused();
    });

    // TODO(shik): Tune the timing for playing video shutter button animation.
    // Currently the |TAKING| state is ended when the file is saved.
    util.bindElementAriaLabelWithState({
      element: videoShutter,
      state: state.State.TAKING,
      onLabel: 'record_video_stop_button',
      offLabel: 'record_video_start_button',
    });
    util.bindElementAriaLabelWithState({
      element: pauseShutter,
      state: state.State.RECORDING_PAUSED,
      onLabel: 'record_video_resume_button',
      offLabel: 'record_video_pause_button',
    });

    // Monitor the states to stop camera when locked/minimized.
    browserProxy.addOnLockListener((isLocked) => {
      this.locked_ = isLocked;
      if (this.locked_) {
        this.start();
      }
    });

    browserProxy.addOnMinimizedListener(() => {
      this.start();
    });

    document.addEventListener('visibilitychange', () => {
      const recording = state.get(state.State.TAKING) && state.get(Mode.VIDEO);
      if (this.isTabletBackground_() && !recording) {
        this.start();
      }
    });
  }

  /**
   * Initializes camera view.
   * @return {!Promise}
   */
  async initialize() {
    const helper = await ChromeHelper.getInstance();

    const setTablet = (isTablet) => state.set(state.State.TABLET, isTablet);
    const isTablet = await helper.initTabletModeMonitor(setTablet);
    setTablet(isTablet);

    const setScreenOffAuto = (s) => {
      const offAuto = s === chromeosCamera.mojom.ScreenState.OFF_AUTO;
      state.set(state.State.SCREEN_OFF_AUTO, offAuto);
    };
    const screenState = await helper.initScreenStateMonitor(setScreenOffAuto);
    setScreenOffAuto(screenState);

    const updateExternalScreen = (hasExternalScreen) => {
      state.set(state.State.HAS_EXTERNAL_SCREEN, hasExternalScreen);
    };
    const hasExternalScreen =
        await helper.initExternalScreenMonitor(updateExternalScreen);
    updateExternalScreen(hasExternalScreen);

    const checkScreenOff = () => {
      if (this.screenOff_) {
        this.start();
      }
    };

    state.addObserver(state.State.SCREEN_OFF_AUTO, checkScreenOff);
    state.addObserver(state.State.HAS_EXTERNAL_SCREEN, checkScreenOff);
  }

  /**
   * @return {boolean} If the App window is invisible to user with respect to
   * screen off state.
   * @private
   */
  get screenOff_() {
    return state.get(state.State.SCREEN_OFF_AUTO) &&
        !state.get(state.State.HAS_EXTERNAL_SCREEN);
  }

  /**
   * @return {boolean} Returns if window is fully overlapped by other window in
   * both window mode or tablet mode.
   * @private
   */
  get isVisible_() {
    return document.visibilityState !== 'hidden';
  }

  /**
   * @return {boolean} Whether window is put to background in tablet mode.
   * @private
   */
  isTabletBackground_() {
    return state.get(state.State.TABLET) && !this.isVisible_;
  }

  /**
   * Whether app window is suspended.
   * @return {boolean}
   */
  isSuspended() {
    return this.locked_ || browserProxy.isMinimized() ||
        state.get(state.State.SUSPEND) || this.screenOff_ ||
        this.isTabletBackground_();
  }

  /**
   * @override
   */
  focus() {
    // Avoid focusing invisible shutters.
    dom.getAll('button.shutter', HTMLButtonElement)
        .forEach((btn) => btn.offsetParent && btn.focus());
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
   * @param {!metrics.ShutterType} shutterType The shutter is triggered by which
   *     shutter type.
   * @return {?Promise} Promise resolved when take action completes. Returns
   *     null if CCA can't start take action.
   * @protected
   */
  beginTake_(shutterType) {
    if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
      return null;
    }

    state.set(state.State.TAKING, true);
    this.shutterType_ = shutterType;
    this.focus();  // Refocus the visible shutter button for ChromeVox.
    this.take_ = (async () => {
      let hasError = false;
      try {
        await timertick.start();
        await this.modes_.current.startCapture();
      } catch (e) {
        hasError = true;
        if (e && e.message === 'cancel') {
          return;
        }
        console.error(e);
      } finally {
        this.take_ = null;
        state.set(
            state.State.TAKING, false, {hasError, facing: this.facingMode_});
        this.focus();  // Refocus the visible shutter button for ChromeVox.
      }
    })();
    return this.take_;
  }

  /**
   * Ends the current take (or clears scheduled further takes if any.)
   * @return {!Promise} Promise for the operation.
   * @private
   */
  endTake_() {
    timertick.cancel();
    this.modes_.current.stopCapture();
    return Promise.resolve(this.take_);
  }

  /**
   * @override
   */
  async handleResultPhoto({resolution, blob, isVideoSnapshot}, name) {
    metrics.sendCaptureEvent({
      facing: this.facingMode_,
      resolution,
      shutterType: this.shutterType_,
      isVideoSnapshot,
    });
    try {
      await this.resultSaver_.savePhoto(blob, name);
    } catch (e) {
      toast.show('error_msg_save_file_failed');
      throw e;
    }
  }

  /**
   * @override
   */
  createVideoSaver() {
    return this.resultSaver_.startSaveVideo();
  }

  /**
   * @override
   */
  playShutterEffect() {
    sound.play('#sound-shutter');
    util.animateOnce(this.preview_.video);
  }

  /**
   * @override
   */
  getPreviewFrame() {
    return this.preview_.toImage();
  }

  /**
   * @override
   */
  async handleResultVideo({resolution, duration, videoSaver, everPaused}) {
    metrics.sendCaptureEvent({
      facing: this.facingMode_,
      duration,
      resolution,
      shutterType: this.shutterType_,
      everPaused,
    });
    try {
      await this.resultSaver_.finishSaveVideo(videoSaver);
    } catch (e) {
      toast.show('error_msg_save_file_failed');
      throw e;
    }
  }

  /**
   * @override
   */
  layout() {
    this.layout_.update();
  }

  /**
   * @override
   */
  handlingKey(key) {
    if (key === 'Ctrl-R') {
      toast.showDebugMessage(this.preview_.toString());
      return true;
    }
    if ((key === 'AudioVolumeUp' || key === 'AudioVolumeDown') &&
        state.get(state.State.TABLET) && state.get(state.State.STREAMING)) {
      if (state.get(state.State.TAKING)) {
        this.endTake_();
      } else {
        this.beginTake_(metrics.ShutterType.VOLUME_KEY);
      }
      return true;
    }
    return false;
  }

  /**
   * Stops camera and tries to start camera stream again if possible.
   * @return {!Promise<boolean>} Promise resolved to whether start camera
   *     successfully.
   */
  async start() {
    // To prevent multiple callers enter this function at the same time, wait
    // until previous caller resets configuring to null.
    while (this.configuring_ !== null) {
      if (!await this.configuring_) {
        // Retry will be kicked out soon.
        return false;
      }
    }
    state.set(state.State.CAMERA_CONFIGURING, true);
    this.configuring_ = (async () => {
      try {
        if (state.get(state.State.TAKING)) {
          await this.endTake_();
        }
      } finally {
        this.preview_.stop();
      }
      return this.start_();
    })();
    return this.configuring_;
  }

  /**
   * Try start stream reconfiguration with specified mode and device id.
   * @param {?string} deviceId
   * @param {!Mode} mode
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithMode_(deviceId, mode) {
    const deviceOperator = await DeviceOperator.getInstance();
    let resolCandidates = null;
    if (deviceOperator !== null) {
      if (deviceId !== null) {
        resolCandidates = this.modes_.getResolutionCandidates(mode, deviceId);
      } else {
        console.error(
            'Null device id present on HALv3 device. Fallback to v1.');
      }
    }
    if (resolCandidates === null) {
      resolCandidates = this.modes_.getResolutionCandidatesV1(mode, deviceId);
    }
    for (const {resolution: captureR, previewCandidates} of resolCandidates) {
      for (const constraints of previewCandidates) {
        if (this.isSuspended()) {
          throw new CameraSuspendedError();
        }
        try {
          if (deviceOperator !== null) {
            assert(deviceId !== null);
            const optConfigs =
                mode === Mode.VIDEO ? {} : {stillCaptureResolution: captureR};
            await deviceOperator.setStreamConfig(
                deviceId, constraints, optConfigs);
            await deviceOperator.setCaptureIntent(
                deviceId, this.modes_.getCaptureIntent(mode));
          }
          const stream = await navigator.mediaDevices.getUserMedia(constraints);
          await this.preview_.start(stream);
          this.facingMode_ = await this.options_.updateValues(stream);
          await this.modes_.updateModeSelectionUI(deviceId);
          await this.modes_.updateMode(
              mode, stream, this.facingMode_, deviceId, captureR);
          nav.close(ViewName.WARNING, 'no-camera');
          return true;
        } catch (e) {
          this.preview_.stop();
          console.error(e);
        }
      }
    }
    return false;
  }

  /**
   * Try start stream reconfiguration with specified device id.
   * @param {?string} deviceId
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithDevice_(deviceId) {
    const supportedModes = await this.modes_.getSupportedModes(deviceId);
    const modes = this.modes_.getModeCandidates().filter(
        (m) => supportedModes.includes(m));
    for (const mode of modes) {
      if (await this.startWithMode_(deviceId, mode)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Starts camera configuration process.
   * @return {!Promise<boolean>} Resolved to boolean for whether the
   *     configuration is succeeded or kicks out another round of
   *     reconfiguration.
   * @private
   */
  async start_() {
    try {
      await this.infoUpdater_.lockDeviceInfo(async () => {
        if (!this.isSuspended()) {
          for (const id of await this.options_.videoDeviceIds()) {
            if (await this.startWithDevice_(id)) {
              // Make the different active camera announced by screen reader.
              const currentId = this.options_.currentDeviceId;
              assert(currentId !== null);
              if (currentId === this.activeDeviceId_) {
                return;
              }
              this.activeDeviceId_ = currentId;
              const info = await this.infoUpdater_.getDeviceInfo(currentId);
              if (info !== null) {
                toast.speak('status_msg_camera_switched', info.label);
              }
              return;
            }
          }
        }
        throw new CameraSuspendedError();
      });
      this.configuring_ = null;
      state.set(state.State.CAMERA_CONFIGURING, false);

      return true;
    } catch (error) {
      this.activeDeviceId_ = null;
      if (!(error instanceof CameraSuspendedError)) {
        console.error(error);
        nav.open(ViewName.WARNING, 'no-camera');
      }
      // Schedule to retry.
      if (this.retryStartTimeout_) {
        clearTimeout(this.retryStartTimeout_);
        this.retryStartTimeout_ = null;
      }
      this.retryStartTimeout_ = setTimeout(() => {
        this.configuring_ = this.start_();
      }, 100);

      this.perfLogger_.interrupt();
      return false;
    }
  }
}
