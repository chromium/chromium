// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from '../../../async_job_queue.js';
import {assert, assertString} from '../../../chrome_util.js';
import * as dom from '../../../dom.js';
// eslint-disable-next-line no-unused-vars
import * as h264 from '../../../h264.js';
import {Filenamer} from '../../../models/file_namer.js';
import * as loadTimeData from '../../../models/load_time_data.js';
import {
  VideoSaver,  // eslint-disable-line no-unused-vars
} from '../../../models/video_saver.js';
import * as sound from '../../../sound.js';
import * as state from '../../../state.js';
import * as toast from '../../../toast.js';
import {
  CanceledError,
  Facing,  // eslint-disable-line no-unused-vars
  PerfEvent,
  Resolution,
  ResolutionList,  // eslint-disable-line no-unused-vars
} from '../../../type.js';
import * as util from '../../../util.js';
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
]);

/**
 * @type {?h264.EncoderParameters}
 */
let avc1Parameters = null;

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
 * @param {!Event} event The 'beforeunload' event.
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

  /**
   * Gets frame image blob from current preview.
   * @return {!Promise<!Blob>}
   * @abstract
   */
  getPreviewFrame() {}
}

/**
 * Video mode capture controller.
 */
export class Video extends ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!VideoHandler} handler
   */
  constructor(stream, facing, handler) {
    super(stream, facing, null);

    /**
     * @const {!VideoHandler}
     * @private
     */
    this.handler_ = handler;

    /**
     * MediaRecorder object to record motion pictures.
     * @type {?MediaRecorder}
     * @private
     */
    this.mediaRecorder_ = null;

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
   * Takes a video snapshot during recording.
   * @return {!Promise} Promise resolved when video snapshot is finished.
   */
  takeSnapshot() {
    const doSnapshot = async () => {
      const blob = await this.handler_.getPreviewFrame();
      this.handler_.playShutterEffect();
      const {width, height} = await util.blobToImage(blob);
      const imageName = (new Filenamer()).newImageName();
      await this.handler_.handleResultPhoto(
          {
            resolution: new Resolution(width, height),
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
    const preference = encoderPreference.get(loadTimeData.getBoard());
    if (preference === undefined) {
      return null;
    }
    const {profile, multiplier} = preference;
    const {width, height, frameRate} =
        this.stream_.getVideoTracks()[0].getSettings();
    const resolution = new Resolution(width, height);
    const bitrate = resolution.area * multiplier;
    const level = h264.getMinimalLevel(profile, bitrate, frameRate, resolution);
    if (level === null) {
      console.warn(
          `No valid level found for ` +
          `profile: ${h264.getProfileName(profile)} bitrate: ${bitrate}`);
      return null;
    }
    return {profile, level, bitrate};
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
      this.mediaRecorder_ = new MediaRecorder(this.stream_, option);
    } catch (e) {
      toast.show('error_msg_record_start_failed');
      throw e;
    }

    this.recordTime_.start({resume: false});
    let /** ?VideoSaver */ videoSaver = null;
    let /** number */ duration = 0;
    try {
      videoSaver = await this.captureVideo_();
    } catch (e) {
      toast.show('error_msg_empty_recording');
      throw e;
    } finally {
      duration = this.recordTime_.stop({pause: false});
    }
    sound.play(dom.get('#sound-rec-end', HTMLAudioElement));

    const settings = this.stream_.getVideoTracks()[0].getSettings();
    const resolution = new Resolution(settings.width, settings.height);
    state.set(PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, true);
    try {
      await this.handler_.handleResultVideo(new VideoResult(
          {resolution, duration, videoSaver, everPaused: this.everPaused_}));
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing_});
    } catch (e) {
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }

    await this.snapshots_.flush();
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

    return new Promise((resolve, reject) => {
      let noChunk = true;

      const ondataavailable = (event) => {
        if (event.data && event.data.size > 0) {
          noChunk = false;
          saver.write(event.data);
        }
      };
      const onstop = (event) => {
        state.set(state.State.RECORDING, false);
        state.set(state.State.RECORDING_PAUSED, false);
        state.set(state.State.RECORDING_UI_PAUSED, false);

        this.mediaRecorder_.removeEventListener(
            'dataavailable', ondataavailable);
        this.mediaRecorder_.removeEventListener('stop', onstop);

        if (noChunk) {
          reject(new Error('Video blob error.'));
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
  }
}

/**
 * Factory for creating video mode capture object.
 */
export class VideoFactory extends ModeFactory {
  /**
   * @param {!VideoHandler} handler
   */
  constructor(handler) {
    super();

    /**
     * @const {!VideoHandler}
     * @private
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  async prepareDevice(deviceOperator, constraints) {
    const deviceId = assertString(constraints.video.deviceId.exact);
    await deviceOperator.setCaptureIntent(
        deviceId, cros.mojom.CaptureIntent.VIDEO_RECORD);

    let /** number */ minFrameRate = 0;
    let /** number */ maxFrameRate = 0;
    if (constraints.video && constraints.video.frameRate) {
      const frameRate = constraints.video.frameRate;
      if (frameRate.exact) {
        minFrameRate = frameRate.exact;
        maxFrameRate = frameRate.exact;
      } else if (frameRate.min && frameRate.max) {
        minFrameRate = frameRate.min;
        maxFrameRate = frameRate.max;
      }
      // TODO(wtlee): To set the fps range to the default value, we should
      // remove the frameRate from constraints instead of using incomplete
      // range.
    }
    await deviceOperator.setFpsRange(deviceId, minFrameRate, maxFrameRate);
  }

  /**
   * @override
   */
  produce_() {
    return new Video(this.previewStream_, this.facing_, this.handler_);
  }
}
