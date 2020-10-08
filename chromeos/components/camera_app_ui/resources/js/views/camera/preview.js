// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../../browser_proxy/browser_proxy.js';
import * as dom from '../../dom.js';
import {DeviceOperator, parseMetadata} from '../../mojo/device_operator.js';
import * as nav from '../../nav.js';
import * as state from '../../state.js';
import * as util from '../../util.js';

/**
 * Creates a controller for the video preview of Camera view.
 */
export class Preview {
  /**
   * @param {function()} onNewStreamNeeded Callback to request new stream.
   */
  constructor(onNewStreamNeeded) {
    /**
     * @type {function()}
     * @private
     */
    this.onNewStreamNeeded_ = onNewStreamNeeded;

    /**
     * Video element to capture the stream.
     * @type {!HTMLVideoElement}
     * @private
     */
    this.video_ = dom.get('#preview-video', HTMLVideoElement);

    /**
     * Element that shows the preview metadata.
     * @type {!HTMLElement}
     * @private
     */
    this.metadata_ = dom.get('#preview-metadata', HTMLElement);

    /**
     * The observer id for preview metadata.
     * @type {?number}
     * @private
     */
    this.metadataObserverId_ = null;

    /**
     * Current active stream.
     * @type {?MediaStream}
     * @private
     */
    this.stream_ = null;

    /**
     * Watchdog for stream-end.
     * @type {?number}
     * @private
     */
    this.watchdog_ = null;

    /**
     * Promise for the current applying focus.
     * @type {?Promise}
     * @private
     */
    this.focus_ = null;

    /**
     * Timeout for resizing the window.
     * @type {?number}
     * @private
     */
    this.resizeWindowTimeout_ = null;

    window.addEventListener('resize', () => this.onWindowResize_());

    [state.State.EXPERT, state.State.SHOW_METADATA].forEach((s) => {
      state.addObserver(s, this.updateShowMetadata_.bind(this));
    });
  }

  /**
   * Current active stream.
   * @return {?MediaStream}
   */
  get stream() {
    return this.stream_;
  }

  /**
   * @return {!HTMLVideoElement}
   */
  get video() {
    return this.video_;
  }

  /**
   * @override
   */
  toString() {
    const {videoWidth, videoHeight} = this.video_;
    return videoHeight ? `${videoWidth} x ${videoHeight}` : '';
  }

  /**
   * Sets video element's source.
   * @param {!MediaStream} stream Stream to be the source.
   * @return {!Promise} Promise for the operation.
   */
  async setSource_(stream) {
    const node = dom.instantiateTemplate('#preview-video-template');
    const video = dom.getFrom(node, 'video', HTMLVideoElement);
    await new Promise((resolve) => {
      const handler = () => {
        video.removeEventListener('canplay', handler);
        resolve();
      };
      video.addEventListener('canplay', handler);
      video.srcObject = stream;
    });
    await video.play();
    this.video_.parentElement.replaceChild(node, this.video_);
    this.video_.removeAttribute('srcObject');
    this.video_.load();
    this.video_ = video;
    video.addEventListener('resize', () => this.onIntrinsicSizeChanged_());
    video.addEventListener('click', (event) => this.onFocusClicked_(event));
    return this.onIntrinsicSizeChanged_();
  }

  /**
   * Starts the preview with the source stream.
   * @param {!MediaStream} stream Stream to be the source.
   * @return {!Promise} Promise for the operation.
   */
  start(stream) {
    return this.setSource_(stream).then(() => {
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome. As of 55, the event is still broken.
      this.watchdog_ = setInterval(() => {
        // Check if video stream is ended (audio stream may still be live).
        if (!stream.getVideoTracks().length ||
            stream.getVideoTracks()[0].readyState === 'ended') {
          clearInterval(this.watchdog_);
          this.watchdog_ = null;
          this.stream_ = null;
          this.onNewStreamNeeded_();
        }
      }, 100);
      this.stream_ = stream;
      this.updateShowMetadata_();
      state.set(state.State.STREAMING, true);
    });
  }

  /**
   * Stops the preview.
   */
  stop() {
    if (this.watchdog_) {
      clearInterval(this.watchdog_);
      this.watchdog_ = null;
    }
    // Pause video element to avoid black frames during transition.
    this.video_.pause();
    if (this.stream_) {
      this.stream_.getVideoTracks()[0].stop();
      this.stream_ = null;
    }
    state.set(state.State.STREAMING, false);
  }

  /**
   * Checks preview whether to show preview metadata or not.
   * @private
   */
  updateShowMetadata_() {
    if (state.get(state.State.EXPERT) && state.get(state.State.SHOW_METADATA)) {
      this.enableShowMetadata_();
    } else {
      this.disableShowMetadata_();
    }
  }

  /**
   * Creates an image blob of the current frame.
   * @return {!Promise<!Blob>} Promise for the result.
   */
  toImage() {
    const {canvas, ctx} = util.newDrawingCanvas(
        {width: this.video_.videoWidth, height: this.video_.videoHeight});
    ctx.drawImage(this.video_, 0, 0);
    return new Promise((resolve, reject) => {
      canvas.toBlob((blob) => {
        if (blob) {
          resolve(blob);
        } else {
          reject(new Error('Photo blob error.'));
        }
      }, 'image/jpeg');
    });
  }

  /**
   * Displays preview metadata on preview screen.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async enableShowMetadata_() {
    if (!this.stream_) {
      return;
    }

    document.querySelectorAll('.metadata-value').forEach((element) => {
      element.style.display = 'none';
    });

    const displayCategory = (selector, enabled) => {
      document.querySelector(selector).classList.toggle('mode-on', enabled);
    };

    const showValue = (selector, val) => {
      const element = document.querySelector(selector);
      element.style.display = '';
      element.textContent = val;
    };

    const buildInverseTable = (obj, prefix) => {
      const tbl = {};
      for (const [key, val] of Object.entries(obj)) {
        if (!key.startsWith(prefix)) {
          continue;
        }
        if (tbl.hasOwnProperty(val)) {
          console.error(`Duplicated value: ${val}`);
          continue;
        }
        tbl[val] = key.slice(prefix.length);
      }
      return tbl;
    };

    const afStateName = buildInverseTable(
        cros.mojom.AndroidControlAfState, 'ANDROID_CONTROL_AF_STATE_');
    const aeStateName = buildInverseTable(
        cros.mojom.AndroidControlAeState, 'ANDROID_CONTROL_AE_STATE_');
    const awbStateName = buildInverseTable(
        cros.mojom.AndroidControlAwbState, 'ANDROID_CONTROL_AWB_STATE_');
    const aeAntibandingModeName = buildInverseTable(
        cros.mojom.AndroidControlAeAntibandingMode,
        'ANDROID_CONTROL_AE_ANTIBANDING_MODE_');

    const tag = cros.mojom.CameraMetadataTag;
    const metadataEntryHandlers = {
      [tag.ANDROID_LENS_FOCUS_DISTANCE]: ([value]) => {
        if (value === 0) {
          // Fixed-focus camera
          return;
        }
        const focusDistance = (100 / value).toFixed(1);
        showValue('#preview-focus-distance', `${focusDistance} cm`);
      },
      [tag.ANDROID_CONTROL_AF_STATE]: ([value]) => {
        showValue('#preview-af-state', afStateName[value]);
      },
      [tag.ANDROID_SENSOR_SENSITIVITY]: ([value]) => {
        const sensitivity = value;
        showValue('#preview-sensitivity', `ISO ${sensitivity}`);
      },
      [tag.ANDROID_SENSOR_EXPOSURE_TIME]: ([value]) => {
        const shutterSpeed = Math.round(1e9 / value);
        showValue('#preview-exposure-time', `1/${shutterSpeed}`);
      },
      [tag.ANDROID_SENSOR_FRAME_DURATION]: ([value]) => {
        const frameFrequency = Math.round(1e9 / value);
        showValue('#preview-frame-duration', `${frameFrequency} Hz`);
      },
      [tag.ANDROID_CONTROL_AE_ANTIBANDING_MODE]: ([value]) => {
        showValue('#preview-ae-antibanding-mode', aeAntibandingModeName[value]);
      },
      [tag.ANDROID_CONTROL_AE_STATE]: ([value]) => {
        showValue('#preview-ae-state', aeStateName[value]);
      },
      [tag.ANDROID_COLOR_CORRECTION_GAINS]: ([valueRed, , , valueBlue]) => {
        const wbGainRed = valueRed.toFixed(2);
        showValue('#preview-wb-gain-red', `${wbGainRed}x`);
        const wbGainBlue = valueBlue.toFixed(2);
        showValue('#preview-wb-gain-blue', `${wbGainBlue}x`);
      },
      [tag.ANDROID_CONTROL_AWB_STATE]: ([value]) => {
        showValue('#preview-awb-state', awbStateName[value]);
      },
      [tag.ANDROID_CONTROL_AF_MODE]: ([value]) => {
        displayCategory(
            '#preview-af',
            value !==
                cros.mojom.AndroidControlAfMode.ANDROID_CONTROL_AF_MODE_OFF);
      },
      [tag.ANDROID_CONTROL_AE_MODE]: ([value]) => {
        displayCategory(
            '#preview-ae',
            value !==
                cros.mojom.AndroidControlAeMode.ANDROID_CONTROL_AE_MODE_OFF);
      },
      [tag.ANDROID_CONTROL_AWB_MODE]: ([value]) => {
        displayCategory(
            '#preview-awb',
            value !==
                cros.mojom.AndroidControlAwbMode.ANDROID_CONTROL_AWB_MODE_OFF);
      },
    };

    // These should be per session static information and we don't need to
    // recalculate them in every callback.
    const {videoWidth, videoHeight} = this.video_;
    const resolution = `${videoWidth}x${videoHeight}`;
    const videoTrack = this.stream_.getVideoTracks()[0];
    const deviceName = videoTrack.label;

    // Currently there is no easy way to calculate the fps of a video element.
    // Here we use the metadata events to calculate a reasonable approximation.
    const updateFps = (() => {
      const FPS_MEASURE_FRAMES = 100;
      const timestamps = [];
      return () => {
        const now = performance.now();
        timestamps.push(now);
        if (timestamps.length > FPS_MEASURE_FRAMES) {
          timestamps.shift();
        }
        if (timestamps.length === 1) {
          return null;
        }
        return (timestamps.length - 1) / (now - timestamps[0]) * 1000;
      };
    })();

    const callback = (metadata) => {
      showValue('#preview-resolution', resolution);
      showValue('#preview-device-name', deviceName);
      const fps = updateFps();
      if (fps !== null) {
        showValue('#preview-fps', `${fps.toFixed(0)} FPS`);
      }
      for (const entry of metadata.entries) {
        if (entry.count === 0) {
          continue;
        }
        const handler = metadataEntryHandlers[entry.tag];
        if (handler === undefined) {
          continue;
        }
        handler(parseMetadata(entry));
      }
    };

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = videoTrack.getSettings().deviceId;
    this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, cros.mojom.StreamType.PREVIEW_OUTPUT);
  }

  /**
   * Hide display preview metadata on preview screen.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async disableShowMetadata_() {
    if (!this.stream_ || this.metadataObserverId_ === null) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    const isSuccess = await deviceOperator.removeMetadataObserver(
        deviceId, this.metadataObserverId_);
    if (!isSuccess) {
      console.error(`Failed to remove metadata observer with id: ${
          this.metadataObserverId_}`);
    }
    this.metadataObserverId_ = null;
  }

  /**
   * Handles resizing the window for preview's aspect ratio changes.
   * @param {number=} aspectRatio Aspect ratio changed.
   * @return {!Promise}
   * @private
   */
  onWindowResize_(aspectRatio) {
    if (this.resizeWindowTimeout_) {
      clearTimeout(this.resizeWindowTimeout_);
      this.resizeWindowTimeout_ = null;
    }
    nav.onWindowResized();

    // Resize window for changed preview's aspect ratio or restore window size
    // by the last known window's aspect ratio.
    return new Promise((resolve) => {
             if (aspectRatio) {
               resolve();
             } else {
               this.resizeWindowTimeout_ = setTimeout(() => {
                 this.resizeWindowTimeout_ = null;
                 resolve();
               }, 500);  // Delay further resizing for smooth UX.
             }
           })
        .then(() => {
          // Resize window by aspect ratio only if it's not maximized or
          // fullscreen.
          if (browserProxy.isFullscreenOrMaximized()) {
            return;
          }
          return browserProxy.fitWindow();
        });
  }

  /**
   * Handles changed intrinsic size (first loaded or orientation changes).
   * @return {!Promise}
   * @private
   */
  async onIntrinsicSizeChanged_() {
    if (this.video_.videoWidth && this.video_.videoHeight) {
      await this.onWindowResize_(
          this.video_.videoWidth / this.video_.videoHeight);
    }
    this.cancelFocus_();
  }

  /**
   * Handles clicking for focus.
   * @param {!Event} event Click event.
   * @private
   */
  onFocusClicked_(event) {
    this.cancelFocus_();

    // Normalize to square space coordinates by W3C spec.
    const x = event.offsetX / this.video_.offsetWidth;
    const y = event.offsetY / this.video_.offsetHeight;
    const constraints = {advanced: [{pointsOfInterest: [{x, y}]}]};
    const track = this.video_.srcObject.getVideoTracks()[0];
    const focus =
        track.applyConstraints(constraints)
            .then(() => {
              if (focus !== this.focus_) {
                return;  // Focus was cancelled.
              }
              const aim = dom.get('#preview-focus-aim', HTMLObjectElement);
              const clone = aim.cloneNode(true);
              clone.style.left = `${event.offsetX + this.video_.offsetLeft}px`;
              clone.style.top = `${event.offsetY + this.video_.offsetTop}px`;
              clone.hidden = false;
              aim.parentElement.replaceChild(clone, aim);
            })
            .catch(console.error);
    this.focus_ = focus;
  }

  /**
   * Cancels the current applying focus.
   * @private
   */
  cancelFocus_() {
    this.focus_ = null;
    const aim = dom.get('#preview-focus-aim', HTMLObjectElement);
    aim.hidden = true;
  }
}
