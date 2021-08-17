// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from '../../../async_job_queue.js';
import {assert, assertInstanceof} from '../../../chrome_util.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {
  CaptureStream,  // eslint-disable-line no-unused-vars
  StreamManager,
} from '../../../device/stream_manager.js';
import * as dom from '../../../dom.js';
import {reportError} from '../../../error.js';
// eslint-disable-next-line no-unused-vars
import * as h264 from '../../../h264.js';
import {I18nString} from '../../../i18n_string.js';
import {Filenamer} from '../../../models/file_namer.js';
import * as loadTimeData from '../../../models/load_time_data.js';
import {
  VideoSaver,  // eslint-disable-line no-unused-vars
} from '../../../models/video_saver.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import * as sound from '../../../sound.js';
import * as state from '../../../state.js';
import * as toast from '../../../toast.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,  // eslint-disable-line no-unused-vars
  NoChunkError,
  PerfEvent,
  Resolution,
} from '../../../type.js';
import {WaitableEvent} from '../../../waitable_event.js';

import {ModeBase, ModeFactory} from './mode_base.js';
import {PhotoResult} from './photo.js';  // eslint-disable-line no-unused-vars
import {RecordTime} from './record_time.js';

/**
 * Maps from board name to its default encoding profile and bitrate multiplier.
 * @const {!Map<string, {profile: h264.Profile, multiplier: number}>}
 */
const encoderPreference = new Map([
  ['strongbad', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['trogdor', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['dedede', {profile: h264.Profile.HIGH, multiplier: 8}],
  ['volteer', {profile: h264.Profile.HIGH, multiplier: 8}],
]);

/**
 * @type {?h264.EncoderParameters}
 */
let avc1Parameters = null;

// The minimum duration of videos captured via cca.
const MINIMUM_VIDEO_DURATION_IN_MILLISECONDS = 500;

/**
 * Sets avc1 parameter used in video recording.
 * @param {?h264.EncoderParameters} params
 */
export function setAvc1Parameters(params) {
  avc1Parameters = params;
}

/**
 * Gets video recording MIME type. Mkv with AVC1 is the only preferred format.
 * @param {?h264.EncoderParameters} param
 * @return {string} Video recording MIME type.
 */
function getVideoMimeType(param) {
  let suffix = '';
  if (param !== null) {
    const {profile, level} = param;
    suffix = '.' + profile.toString(16).padStart(2, '0') +
        level.toString(16).padStart(4, '0');
  }
  return `video/x-matroska;codecs=avc1${suffix},pcm`;
}

/**
 * The 'beforeunload' listener which will show confirm dialog when trying to
 * close window.
 * @param {!BeforeUnloadEvent} event The 'beforeunload' event.
 */
function beforeUnloadListener(event) {
  event.preventDefault();
  event.returnValue = '';
}

/**
 * Contains video recording result.
 */
export class VideoResult {
  /**
   * @param {{
   *     resolution: !Resolution,
   *     duration: (number|undefined),
   *     videoSaver: !VideoSaver,
   *     everPaused: boolean,
   * }} params
   */
  constructor({resolution, duration = 0, videoSaver, everPaused}) {
    /**
     * @const {!Resolution}
     * @public
     */
    this.resolution = resolution;

    /**
     * @const {number}
     * @public
     */
    this.duration = duration || 0;

    /**
     * @const {!VideoSaver}
     * @public
     */
    this.videoSaver = videoSaver;

    /**
     * @const {boolean}
     * @public
     */
    this.everPaused = everPaused;
  }
}

/**
 * Provides functions with external dependency used by video mode and handles
 * the captured result video.
 * @interface
 */
export class VideoHandler {
  /**
   * Creates VideoSaver to save video capture result.
   * @return {!Promise<!VideoSaver>}
   * @abstract
   */
  createVideoSaver() {}

  /**
   * Handles the result video.
   * @param {!VideoResult} video Captured video result.
   * @return {!Promise}
   * @abstract
   */
  handleResultVideo(video) {}

  /**
   * Handles the result video snapshot.
   * @param {!PhotoResult} photo photo Captured video snapshot photo.
   * @param {string} name Name of the video snapshot result to be saved as.
   * @return {!Promise}
   * @abstract
   */
  handleResultPhoto(photo, name) {}

  /**
   * Plays UI effect when doing video snapshot.
   */
  playShutterEffect() {}
}

/**
 * Video mode capture controller.
 */
export class Video extends ModeBase {
  /**
   * @param {!MediaStream} stream Preview stream.
   * @param {?StreamConstraints} captureConstraints
   * @param {?Resolution} captureResolution
   * @param {!Facing} facing
   * @param {!VideoHandler} handler
   */
  constructor(stream, captureConstraints, captureResolution, facing, handler) {
    super(stream, facing);

    /**
     * @const {?StreamConstraints}
     * @private
     */
    this.captureConstraints_ = captureConstraints;

    /**
     * @const {!Resolution}
     * @private
     */
    this.captureResolution_ = (() => {
      if (captureResolution !== null) {
        return captureResolution;
      }
      const {width, height} = stream.getVideoTracks()[0].getSettings();
      return new Resolution(width, height);
    })();

    /**
     * @const {!VideoHandler}
     * @private
     */
    this.handler_ = handler;

    /**
     * @type {?CaptureStream}
     * @private
     */
    this.captureStream_ = null;

    /**
     * MediaRecorder object to record motion pictures.
     * @type {?MediaRecorder}
     * @private
     */
    this.mediaRecorder_ = null;

    /**
     * @type {?CrosImageCapture}
     * @private
     */
    this.crosImageCapture_ = null;

    /**
     * Record-time for the elapsed recording time.
     * @type {!RecordTime}
     * @private
     */
    this.recordTime_ = new RecordTime();

    /**
     * Queueing all taking video snapshot jobs requested in a single recording.
     * @type {!AsyncJobQueue}
     * @private
     */
    this.snapshots_ = new AsyncJobQueue();

    /**
     * Promise for process of toggling video pause/resume. Sets to null if CCA
     * is already paused or resumed.
     * @type {?Promise}
     * @private
     */
    this.togglePaused_ = null;

    /**
     * Whether current recording ever paused/resumed before it ended.
     */
    this.everPaused_ = false;
  }

  /**
   * @override
   */
  async clear() {
    await this.stopCapture();
    if (this.captureStream_ !== null) {
      await this.captureStream_.close();
    }
  }

  /**
   * Takes a video snapshot during recording.
   * @return {!Promise} Promise resolved when video snapshot is finished.
   */
  takeSnapshot() {
    const doSnapshot = async () => {
      const blob = await this.crosImageCapture_.grabJpegFrame();

      this.handler_.playShutterEffect();
      const imageName = (new Filenamer()).newImageName();
      await this.handler_.handleResultPhoto(
          {
            resolution: this.captureResolution_,
            blob,
            isVideoSnapshot: true,
          },
          imageName);
    };
    this.snapshots_.push(doSnapshot);
    return this.snapshots_.flush();
  }

  /**
   * Toggles pause/resume state of video recording.
   * @return {!Promise} Promise resolved when recording is paused/resumed.
   */
  async togglePaused() {
    if (!state.get(state.State.RECORDING)) {
      return;
    }
    if (this.togglePaused_ !== null) {
      return this.togglePaused_;
    }
    this.everPaused_ = true;
    const waitable = new WaitableEvent();
    this.togglePaused_ = waitable.wait();

    assert(this.mediaRecorder_.state !== 'inactive');
    const toBePaused = this.mediaRecorder_.state !== 'paused';
    const toggledEvent = toBePaused ? 'pause' : 'resume';
    const onToggled = () => {
      this.mediaRecorder_.removeEventListener(toggledEvent, onToggled);
      state.set(state.State.RECORDING_PAUSED, toBePaused);
      this.togglePaused_ = null;
      waitable.signal();
    };
    const playEffect = async () => {
      state.set(state.State.RECORDING_UI_PAUSED, toBePaused);
      await sound.play(dom.get(
          toBePaused ? '#sound-rec-pause' : '#sound-rec-start',
          HTMLAudioElement));
    };

    this.mediaRecorder_.addEventListener(toggledEvent, onToggled);
    if (toBePaused) {
      waitable.wait().then(playEffect);
      this.recordTime_.stop({pause: true});
      this.mediaRecorder_.pause();
    } else {
      await playEffect();
      this.recordTime_.start({resume: true});
      this.mediaRecorder_.resume();
    }

    return waitable.wait();
  }

  /**
   * @return {?h264.EncoderParameters}
   * @private
   */
  getEncoderParameters_() {
    if (avc1Parameters !== null) {
      return avc1Parameters;
    }
    const preference = encoderPreference.get(loadTimeData.getBoard()) ||
        {profile: h264.Profile.HIGH, multiplier: 2};
    const {profile, multiplier} = preference;
    const {width, height, frameRate} = this.getVideoTrack_().getSettings();
    const resolution = new Resolution(width, height);
    const bitrate = resolution.area * multiplier;
    const level = h264.getMinimalLevel(profile, bitrate, frameRate, resolution);
    if (level === null) {
      reportError(
          ErrorType.NO_AVAILABLE_LEVEL, ErrorLevel.WARNING,
          new Error(
              `No valid level found for ` +
              `profile: ${h264.getProfileName(profile)} bitrate: ${bitrate}`));
      return null;
    }
    return {profile, level, bitrate};
  }

  /**
   * @return {!MediaStream}
   * @private
   */
  getRecordingStream_() {
    if (this.captureStream_ !== null) {
      return this.captureStream_.stream;
    }
    return this.stream_;
  }

  /**
   * Gets video track of recording stream.
   * @return {!MediaStreamTrack}
   */
  getVideoTrack_() {
    return this.getRecordingStream_().getVideoTracks()[0];
  }

  /**
   * @override
   */
  async start_() {
    this.snapshots_ = new AsyncJobQueue();
    this.togglePaused_ = null;
    this.everPaused_ = false;

    const isSoundEnded =
        await sound.play(dom.get('#sound-rec-start', HTMLAudioElement));
    if (!isSoundEnded) {
      throw new CanceledError('Recording sound is canceled');
    }

    if (this.captureConstraints_ !== null && this.captureStream_ === null) {
      this.captureStream_ = await StreamManager.getInstance().openCaptureStream(
          this.captureConstraints_);
    }
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ = new CrosImageCapture(this.getVideoTrack_());
    }

    try {
      const param = this.getEncoderParameters_();
      const mimeType = getVideoMimeType(param);
      if (!MediaRecorder.isTypeSupported(mimeType)) {
        throw new Error(
            `The preferred mimeType "${mimeType}" is not supported.`);
      }
      const option = {mimeType};
      if (param !== null) {
        option.videoBitsPerSecond = param.bitrate;
      }
      this.mediaRecorder_ =
          new MediaRecorder(this.getRecordingStream_(), option);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_RECORD_START_FAILED);
      throw e;
    }

    this.recordTime_.start({resume: false});
    let /** ?VideoSaver */ videoSaver = null;
    const isVideoTooShort = () => this.recordTime_.inMilliseconds() <
        MINIMUM_VIDEO_DURATION_IN_MILLISECONDS;

    try {
      try {
        videoSaver = await this.captureVideo_();
      } finally {
        this.recordTime_.stop({pause: false});
        sound.play(dom.get('#sound-rec-end', HTMLAudioElement));
        await this.snapshots_.flush();
      }
    } catch (e) {
      // Tolerates the error if it is due to the very short duration. Reports
      // for other errors.
      if (!(e instanceof NoChunkError && isVideoTooShort())) {
        toast.show(I18nString.ERROR_MSG_EMPTY_RECORDING);
        throw e;
      }
    }

    if (isVideoTooShort()) {
      toast.show(I18nString.ERROR_MSG_VIDEO_TOO_SHORT);
      if (videoSaver !== null) {
        await videoSaver.cancel();
      }
      return;
    }

    state.set(PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, true);
    try {
      await this.handler_.handleResultVideo(new VideoResult({
        resolution: this.captureResolution_,
        duration: this.recordTime_.inMinutes(),
        videoSaver,
        everPaused: this.everPaused_,
      }));
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false,
          {resolution: this.captureResolution_, facing: this.facing_});
    } catch (e) {
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
  }

  /**
   * @override
   */
  stop_() {
    sound.cancel(dom.get('#sound-rec-start', HTMLAudioElement));

    if (this.mediaRecorder_ &&
        (this.mediaRecorder_.state === 'recording' ||
         this.mediaRecorder_.state === 'paused')) {
      this.mediaRecorder_.stop();
      window.removeEventListener('beforeunload', beforeUnloadListener);
    }
  }

  /**
   * Starts recording and waits for stop recording event triggered by stop
   * shutter.
   * @return {!Promise<!VideoSaver>} Saves recorded video.
   * @private
   */
  async captureVideo_() {
    const saver = await this.handler_.createVideoSaver();

    try {
      await new Promise((resolve, reject) => {
        let noChunk = true;

        const ondataavailable = (event) => {
          if (event.data && event.data.size > 0) {
            noChunk = false;
            saver.write(event.data);
          }
        };
        const onstop = async (event) => {
          state.set(state.State.RECORDING, false);
          state.set(state.State.RECORDING_PAUSED, false);
          state.set(state.State.RECORDING_UI_PAUSED, false);

          this.mediaRecorder_.removeEventListener(
              'dataavailable', ondataavailable);
          this.mediaRecorder_.removeEventListener('stop', onstop);

          if (noChunk) {
            reject(new NoChunkError());
          } else {
            // TODO(yuli): Handle insufficient storage.
            resolve(saver);
          }
        };
        const onstart = () => {
          state.set(state.State.RECORDING, true);
          this.mediaRecorder_.removeEventListener('start', onstart);
        };
        this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
        this.mediaRecorder_.addEventListener('stop', onstop);
        this.mediaRecorder_.addEventListener('start', onstart);

        window.addEventListener('beforeunload', beforeUnloadListener);

        this.mediaRecorder_.start(100);
        state.set(state.State.RECORDING_PAUSED, false);
        state.set(state.State.RECORDING_UI_PAUSED, false);
      });
      return saver;
    } catch (e) {
      await saver.cancel();
      throw e;
    }
  }
}

/**
 * Factory for creating video mode capture object.
 */
export class VideoFactory extends ModeFactory {
  /**
   * @param {!StreamConstraints} constraints Constraints for preview
   *     stream.
   * @param {?Resolution} captureResolution
   * @param {!VideoHandler} handler
   */
  constructor(constraints, captureResolution, handler) {
    super(constraints, captureResolution);

    /**
     * @const {!VideoHandler}
     * @private
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  produce() {
    let captureConstraints = null;
    if (state.get(state.State.ENABLE_MULTISTREAM_RECORDING)) {
      const {width, height} =
          assertInstanceof(this.captureResolution_, Resolution);
      captureConstraints = {
        deviceId: this.constraints_.deviceId,
        audio: this.constraints_.audio,
        video: {
          frameRate: this.constraints_.video.frameRate,
          width,
          height,
        },
      };
    }
    return new Video(
        this.previewStream_, captureConstraints, this.captureResolution_,
        this.facing_, this.handler_);
  }
}
