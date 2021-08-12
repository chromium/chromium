// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../animation.js';
import {
  assert,
  assertInstanceof,
  assertString,
} from '../chrome_util.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../device/constraints_preferrer.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
import * as dom from '../dom.js';
import * as error from '../error.js';
import {I18nString} from '../i18n_string.js';
import * as metrics from '../metrics.js';
import * as loadTimeData from '../models/load_time_data.js';
import * as localStorage from '../models/local_storage.js';
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
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  Resolution,
  ViewName,
} from '../type.js';
import * as util from '../util.js';
import {windowController} from '../window_controller.js';

import {Layout} from './camera/layout.js';
import {
  Modes,
  PhotoHandler,    // eslint-disable-line no-unused-vars
  ScannerHandler,  // eslint-disable-line no-unused-vars
  setAvc1Parameters,
  Video,
  VideoHandler,  // eslint-disable-line no-unused-vars
} from './camera/mode/index.js';
import {Options} from './camera/options.js';
import {Preview} from './camera/preview.js';
import {ScannerOptions} from './camera/scanner_options.js';
import * as timertick from './camera/timertick.js';
import {VideoEncoderOptions} from './camera/video_encoder_options.js';
import {PTZPanel} from './ptz_panel.js';
import {PrimarySettings} from './settings.js';
import {PTZPanelOptions, View} from './view.js';
import {WarningType} from './warning.js';

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
 * @implements {ScannerHandler}
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
     * @const {!Array<!View>}
     * @private
     */
    this.subViews_ = [
      new PrimarySettings(infoUpdater, photoPreferrer, videoPreferrer),
      new PTZPanel(),
    ];

    /**
     * Layout handler for the camera view.
     * @type {!Layout}
     * @private
     */
    this.layout_ = new Layout();

    /**
     * @type {!ScannerOptions}
     * @private
     */
    this.scannerOptions_ =
        new ScannerOptions(this.start.bind(this), this.infoUpdater_);

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
     * @type {!VideoEncoderOptions}
     * @private
     */
    this.videoEncoderOptions_ =
        new VideoEncoderOptions((parameters) => setAvc1Parameters(parameters));

    /**
     * Clock-wise rotation that needs to be applied to the recorded video in
     * order for the video to be replayed in upright orientation.
     * @type {number}
     * @private
     */
    this.outputVideoRotation_ = 0;

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
     * The last time of all screen state turning from OFF to ON during the app
     * execution. Sets to -Infinity for no such time since app is opened.
     * @type {number}
     * @private
     */
    this.lastScreenOnTime_ = -Infinity;

    /**
     * Modes for the camera.
     * @type {!Modes}
     * @private
     */
    this.modes_ = new Modes(
        this.defaultMode_, photoPreferrer, videoPreferrer,
        this.start.bind(this), this, this, this);

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
     * @type {!HTMLElement}
     * @private
     */
    this.banner_ = dom.get('#banner', HTMLElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.ptzToast_ = dom.get('#ptz-toast', HTMLElement);

    /**
     * @type {!HTMLButtonElement}
     */
    this.openPTZPanel_ = dom.get('#open-ptz-panel', HTMLButtonElement);

    /**
     * @const {!Set<function(): *>}
     * @private
     */
    this.configureCompleteListener_ = new Set();

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

    dom.get('#start-takephoto', HTMLButtonElement)
        .addEventListener('click', (e) => {
          const mouseEvent = assertInstanceof(e, MouseEvent);
          this.beginTake_(getShutterType(mouseEvent));
        });

    dom.get('#stop-takephoto', HTMLButtonElement)
        .addEventListener('click', () => this.endTake_());

    const videoShutter = dom.get('#recordvideo', HTMLButtonElement);
    videoShutter.addEventListener('click', (e) => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake_(getShutterType(assertInstanceof(e, MouseEvent)));
      } else {
        this.endTake_();
      }
    });

    dom.get('#video-snapshot', HTMLButtonElement)
        .addEventListener('click', () => {
          const videoMode = assertInstanceof(this.modes_.current, Video);
          videoMode.takeSnapshot();
        });

    const pauseShutter = dom.get('#pause-recordvideo', HTMLButtonElement);
    pauseShutter.addEventListener('click', () => {
      const videoMode = assertInstanceof(this.modes_.current, Video);
      videoMode.togglePaused();
    });

    // TODO(shik): Tune the timing for playing video shutter button animation.
    // Currently the |TAKING| state is ended when the file is saved.
    util.bindElementAriaLabelWithState({
      element: videoShutter,
      state: state.State.TAKING,
      onLabel: I18nString.RECORD_VIDEO_STOP_BUTTON,
      offLabel: I18nString.RECORD_VIDEO_START_BUTTON,
    });
    util.bindElementAriaLabelWithState({
      element: pauseShutter,
      state: state.State.RECORDING_PAUSED,
      onLabel: I18nString.RECORD_VIDEO_RESUME_BUTTON,
      offLabel: I18nString.RECORD_VIDEO_PAUSE_BUTTON,
    });

    dom.get('#banner-close', HTMLButtonElement)
        .addEventListener('click', () => {
          animate.cancel(this.banner_);
        });

    this.initOpenPTZPanel_();

    // Monitor the states to stop camera when locked/minimized.
    const idleDetector = new window.IdleDetector();
    idleDetector.addEventListener('change', () => {
      this.locked_ = idleDetector.screenState === 'locked';
      if (this.locked_) {
        this.start();
      }
    });
    idleDetector.start().catch((e) => {
      error.reportError(
          ErrorType.IDLE_DETECTOR_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
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

    const handleScreenStateChange = () => {
      if (this.screenOff_) {
        this.start();
      } else {
        this.lastScreenOnTime_ = performance.now();
      }
    };

    state.addObserver(state.State.SCREEN_OFF_AUTO, handleScreenStateChange);
    state.addObserver(state.State.HAS_EXTERNAL_SCREEN, handleScreenStateChange);

    state.addObserver(state.State.ENABLE_MULTISTREAM_RECORDING, () => {
      this.start();
    });

    this.initVideoEncoderOptions_();
    await this.initScannerMode_();
  }

  /**
   * @private
   */
  initOpenPTZPanel_() {
    this.openPTZPanel_.addEventListener('click', () => {
      nav.open(ViewName.PTZ_PANEL, new PTZPanelOptions({
                 stream: assertInstanceof(this.preview_.stream, MediaStream),
                 vidPid: this.preview_.getVidPid(),
                 resetPTZ: () => this.preview_.resetPTZ(),
               }));
      highlight(false);
    });

    // Highlight effect for PTZ button.
    const highlight = (enabled) => {
      this.ptzToast_.classList.toggle('hidden', !enabled);
      this.openPTZPanel_.classList.toggle('rippling', enabled);
      if (enabled) {
        this.ptzToast_.focus();
        setTimeout(() => highlight(false), 10000);
      }
    };

    this.addConfigureCompleteListener_(async () => {
      if (!state.get(state.State.ENABLE_PTZ)) {
        highlight(false);
        return;
      }

      const ptzToastKey = 'isPTZToastShown';
      if (localStorage.getBool(ptzToastKey)) {
        return;
      }
      localStorage.set(ptzToastKey, true);

      const {bottom, right} =
          dom.get('#open-ptz-panel', HTMLButtonElement).getBoundingClientRect();
      this.ptzToast_.style.bottom = `${window.innerHeight - bottom}px`;
      this.ptzToast_.style.left = `${right + 20}px`;
      highlight(true);
    });
  }

  /**
   * @private
   */
  initVideoEncoderOptions_() {
    const options = this.videoEncoderOptions_;
    this.addConfigureCompleteListener_(() => {
      if (state.get(Mode.VIDEO)) {
        const {width, height, frameRate} =
            this.preview_.stream.getVideoTracks()[0].getSettings();
        options.updateValues(new Resolution(width, height), frameRate);
      }
    });
    options.initialize();
  }

  /**
   * @private
   */
  async initScannerMode_() {
    const helper = await ChromeHelper.getInstance();
    state.set(
        state.State.PLATFORM_SUPPORT_SCAN_DOCUMENT,
        await helper.isDocumentModeSupported());
  }

  /**
   * @param {function(): *} listener
   * @private
   */
  addConfigureCompleteListener_(listener) {
    this.configureCompleteListener_.add(listener);
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
    return this.locked_ || windowController.isMinimized() ||
        state.get(state.State.SUSPEND) || this.screenOff_ ||
        this.isTabletBackground_();
  }

  /**
   * @override
   */
  getSubViews() {
    return this.subViews_;
  }

  /**
   * @override
   */
  focus() {
    const focusOnShutterButton = () => {
      // Avoid focusing invisible shutters.
      dom.getAll('button.shutter', HTMLButtonElement)
          .forEach((btn) => btn.offsetParent && btn.focus());
    };
    (async () => {
      const shown = localStorage.getBool('isFolderChangeMsgShown');
      await this.configuring_;
      if (!shown) {
        localStorage.set('isFolderChangeMsgShown', true);
        await animate.play(this.banner_);
      }
      focusOnShutterButton();
    })();
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
    if (state.get(state.State.CAMERA_CONFIGURING) ||
        state.get(state.State.TAKING)) {
      return null;
    }

    state.set(state.State.TAKING, true);
    this.shutterType_ = shutterType;
    this.focus();  // Refocus the visible shutter button for ChromeVox.
    this.take_ = (async () => {
      let hasError = false;
      try {
        // Record and keep the rotation only at the instance the user starts the
        // capture. Users may change the device orientation while taking video.
        const cameraFrameRotation = await (async () => {
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return 0;
          }
          assert(this.activeDeviceId_ !== null);
          return await deviceOperator.getCameraFrameRotation(
              this.activeDeviceId_);
        })();
        // Translate the camera frame rotation back to the UI rotation, which is
        // what we need to rotate the captured video with.
        this.outputVideoRotation_ = (360 - cameraFrameRotation) % 360;
        await timertick.start();
        await this.modes_.current.startCapture();
      } catch (e) {
        hasError = true;
        if (e instanceof CanceledError) {
          return;
        }
        error.reportError(
            ErrorType.START_CAPTURE_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
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
   * @return {number}
   */
  getPreviewAspectRatio() {
    const {videoWidth, videoHeight} = this.preview_.video;
    return videoWidth / videoHeight;
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
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * @override
   */
  createVideoSaver() {
    return this.resultSaver_.startSaveVideo(this.outputVideoRotation_);
  }

  /**
   * @override
   */
  playShutterEffect() {
    sound.play(dom.get('#sound-shutter', HTMLAudioElement));
    animate.play(this.preview_.video);
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
  waitPreviewReady() {
    return this.preview_.waitReadyForTakePhoto();
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
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
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
        await this.stopStreams_();
      }
      return this.start_();
    })();
    return this.configuring_;
  }

  /**
   * Try start stream reconfiguration with specified mode and device id.
   * @param {?string} deviceId Null if the default camera should be started.
   * @param {!Mode} mode
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithMode_(deviceId, mode) {
    const deviceOperator = await DeviceOperator.getInstance();
    state.set(state.State.USE_FAKE_CAMERA, deviceOperator === null);
    let resolCandidates;
    if (deviceOperator) {
      resolCandidates =
          this.modes_.getResolutionCandidates(mode, assertString(deviceId));
    } else {
      resolCandidates = this.modes_.getFakeResolutionCandidates(mode, deviceId);
    }
    for (const {resolution: captureR, previewCandidates} of resolCandidates) {
      for (const constraints of previewCandidates) {
        if (this.isSuspended()) {
          throw new CameraSuspendedError();
        }
        const factory = this.modes_.getModeFactory(mode);
        try {
          await factory.prepareDevice(constraints, captureR);

          // Sets 2500 ms delay between screen resumed and open camera preview.
          // TODO(b/173679752): Removes this workaround after fix delay on
          // kernel side.
          if (loadTimeData.getBoard() === 'zork') {
            const screenOnTime = performance.now() - this.lastScreenOnTime_;
            const delay = 2500 - screenOnTime;
            if (delay > 0) {
              await util.sleep(delay);
            }
          }
          const stream = await this.preview_.open(constraints);
          this.facingMode_ = this.preview_.getFacing();

          let enablePTZ = this.preview_.isSupportPTZ();
          if (enablePTZ) {
            const modeSupport = deviceOperator === null ||
                this.modes_.isSupportPTZ(
                    mode,
                    captureR,
                    this.preview_.getResolution(),
                );
            if (!modeSupport) {
              await this.preview_.resetPTZ();
              enablePTZ = false;
            }
          }
          state.set(state.State.ENABLE_PTZ, enablePTZ);

          this.options_.updateValues(stream, this.facingMode_);
          factory.setPreviewStream(stream);
          factory.setFacing(this.facingMode_);

          await this.modes_.updateModeSelectionUI(deviceId);
          await this.modes_.updateMode(
              mode, factory, stream, this.facingMode_, deviceId, captureR);
          await this.scannerOptions_.initialize(this.preview_.video);
          for (const l of this.configureCompleteListener_) {
            l();
          }
          nav.close(ViewName.WARNING, WarningType.NO_CAMERA);
          return true;
        } catch (e) {
          await factory.clear();
          await this.stopStreams_();

          let errorToReport = e;
          // Since OverconstrainedError is not an Error instance.
          if (e instanceof OverconstrainedError) {
            errorToReport =
                new Error(`${e.message} (constraint = ${e.constraint})`);
            errorToReport.name = 'OverconstrainedError';
          }
          error.reportError(
              ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR,
              assertInstanceof(errorToReport, Error));
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
                toast.speak(I18nString.STATUS_MSG_CAMERA_SWITCHED, info.label);
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
    } catch (e) {
      this.activeDeviceId_ = null;
      if (!(e instanceof CameraSuspendedError)) {
        error.reportError(
            ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
        nav.open(ViewName.WARNING, WarningType.NO_CAMERA);
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

  /**
   * Stop extra stream and preview stream.
   * @private
   */
  async stopStreams_() {
    // Stopping preview will wait device close. Therefore, we clear
    // mode before stopping preview to close extra stream first.
    await this.modes_.clear();
    await this.preview_.close();
    await this.scannerOptions_.uninitialize();
  }
}
