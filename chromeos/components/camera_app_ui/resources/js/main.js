// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  AppWindow,  // eslint-disable-line no-unused-vars
  getDefaultWindowSize,
} from './app_window.js';
import {assert, assertInstanceof} from './chrome_util.js';
import {
  PhotoConstraintsPreferrer,
  VideoConstraintsPreferrer,
} from './device/constraints_preferrer.js';
import {DeviceInfoUpdater} from './device/device_info_updater.js';
import * as dom from './dom.js';
import * as error from './error.js';
import * as focusRing from './focus_ring.js';
import {GalleryButton} from './gallerybutton.js';
import {Intent} from './intent.js';
import * as metrics from './metrics.js';
import * as filesystem from './models/file_system.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {notifyCameraResourceReady} from './mojo/device_operator.js';
import * as nav from './nav.js';
import {PerfLogger} from './perf.js';
import {preloadImagesList} from './preload_images.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {ErrorLevel, ErrorType, Mode, PerfEvent, ViewName} from './type.js';
import * as util from './util.js';
import {Camera} from './views/camera.js';
import {CameraIntent} from './views/camera_intent.js';
import {Dialog} from './views/dialog.js';
import {PTZPanel} from './views/ptz_panel.js';
import {
  BaseSettings,
  PrimarySettings,
  ResolutionSettings,
} from './views/settings.js';
import {View} from './views/view.js';
import {Warning, WarningType} from './views/warning.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * The app window instance which is used for communication with Tast tests. For
 * non-test sessions, it should be null.
 * @type {?AppWindow}
 */
const appWindow = window['appWindow'];

/**
 * Creates the Camera App main object.
 */
export class App {
  /**
   * @param {{
   *     perfLogger: !PerfLogger,
   *     intent: ?Intent,
   * }} params
   * @public
   */
  constructor({perfLogger, intent}) {
    /**
     * @type {!PerfLogger}
     * @private
     */
    this.perfLogger_ = perfLogger;

    /**
     * @type {?Intent}
     * @private
     */
    this.intent_ = intent;

    /**
     * @type {!PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ =
        new PhotoConstraintsPreferrer(() => this.cameraView_.start());

    /**
     * @type {!VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ =
        new VideoConstraintsPreferrer(() => this.cameraView_.start());

    /**
     * @type {!DeviceInfoUpdater}
     * @private
     */
    this.infoUpdater_ =
        new DeviceInfoUpdater(this.photoPreferrer_, this.videoPreferrer_);

    /**
     * @type {!GalleryButton}
     * @private
     */
    this.galleryButton_ = new GalleryButton();

    /**
     * @type {!Camera}
     * @private
     */
    this.cameraView_ = (() => {
      if (this.intent_ !== null && this.intent_.shouldHandleResult) {
        state.set(state.State.SHOULD_HANDLE_INTENT_RESULT, true);
        return new CameraIntent(
            this.intent_, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, this.perfLogger_);
      } else {
        const mode = this.intent_ !== null ? this.intent_.mode : Mode.PHOTO;
        return new Camera(
            this.galleryButton_, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, mode, this.perfLogger_);
      }
    })();

    document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

    // Disable the zoom in-out gesture which is triggered by wheel and pinch on
    // trackpad.
    document.body.addEventListener('wheel', (event) => {
      if (event.ctrlKey) {
        event.preventDefault();
      }
    }, {passive: false, capture: true});

    document.title = loadTimeData.getI18nMessage('name');
    util.setupI18nElements(document.body);
    this.setupToggles_();
    this.setupEffect_();
    focusRing.initialize();

    const resolutionSettings = new ResolutionSettings(
        this.infoUpdater_, this.photoPreferrer_, this.videoPreferrer_);

    // Set up views navigation by their DOM z-order.
    nav.setup([
      this.cameraView_,
      new PrimarySettings(),
      new PTZPanel(),
      new BaseSettings(ViewName.GRID_SETTINGS),
      new BaseSettings(ViewName.TIMER_SETTINGS),
      resolutionSettings,
      resolutionSettings.photoResolutionSettings,
      resolutionSettings.videoResolutionSettings,
      new BaseSettings(ViewName.EXPERT_SETTINGS),
      new Warning(),
      new Dialog(ViewName.MESSAGE_DIALOG),
      new View(ViewName.SPLASH),
    ]);

    nav.open(ViewName.SPLASH);
  }

  /**
   * Sets up toggles (checkbox and radio) by data attributes.
   * @private
   */
  setupToggles_() {
    const expert = localStorage.getBool('expert');
    state.set(state.State.EXPERT, expert);
    dom.getAll('input', HTMLInputElement).forEach((element) => {
      element.addEventListener('keypress', (event) => {
        const e = assertInstanceof(event, KeyboardEvent);
        if (util.getShortcutIdentifier(e) === 'Enter') {
          element.click();
        }
      });

      const save = (element) => {
        if (element.dataset['key'] !== undefined) {
          localStorage.set(element.dataset['key'], element.checked);
        }
      };
      element.addEventListener('change', (event) => {
        if (element.dataset['state'] !== undefined) {
          state.set(
              state.assertState(element.dataset['state']), element.checked);
        }
        if (event.isTrusted) {
          save(element);
          if (element.type === 'radio' && element.checked) {
            // Handle unchecked grouped sibling radios.
            const grouped =
                `input[type=radio][name=${element.name}]:not(:checked)`;
            for (const radio of dom.getAll(grouped, HTMLInputElement)) {
              radio.dispatchEvent(new Event('change'));
              save(radio);
            }
          }
        }
      });
      if (element.dataset['key'] !== undefined) {
        // Restore the previously saved state on startup.
        const value =
            localStorage.getBool(element.dataset['key'], element.checked);
        util.toggleChecked(element, value);
      }
    });
  }

  /**
   * Sets up visual effect for all applicable elements.
   * @private
   */
  setupEffect_() {
    dom.getAll('.inkdrop', HTMLElement)
        .forEach((el) => util.setInkdropEffect(el));

    const observer = new MutationObserver((mutationList) => {
      mutationList.forEach((mutation) => {
        assert(mutation.type === 'childList');
        // Only the newly added nodes with inkdrop class are considered here. So
        // simply adding class attribute on existing element will not work.
        for (const node of mutation.addedNodes) {
          if (!(node instanceof HTMLElement)) {
            continue;
          }
          const el = assertInstanceof(node, HTMLElement);
          if (el.classList.contains('inkdrop')) {
            util.setInkdropEffect(el);
          }
        }
      });
    });
    observer.observe(document.body, {
      subtree: true,
      childList: true,
    });
  }

  /**
   * Starts the app by loading the model and opening the camera-view.
   * @return {!Promise}
   */
  async start() {
    document.documentElement.dir = loadTimeData.getTextDirection();
    try {
      await filesystem.initialize();
      const cameraDir = filesystem.getCameraDirectory();
      assert(cameraDir !== null);

      // There are three possible cases:
      // 1. Regular instance
      //      (intent === null)
      // 2. STILL_CAPTURE_CAMREA and VIDEO_CAMERA intents
      //      (intent !== null && shouldHandleResult === false)
      // 3. Other intents
      //      (intent !== null && shouldHandleResult === true)
      // Only (1) and (2) will show gallery button on the UI.
      if (this.intent_ === null || !this.intent_.shouldHandleResult) {
        this.galleryButton_.initialize(cameraDir);
      }
    } catch (error) {
      console.error(error);
      nav.open(ViewName.WARNING, WarningType.FILESYSTEM_FAILURE);
    }

    const showWindow = (async () => {
      // For intent only requiring open camera with specific mode without
      // returning the capture result, finish it directly.
      if (this.intent_ !== null && !this.intent_.shouldHandleResult) {
        this.intent_.finish();
      }
    })();

    const cameraResourceInitialized = new WaitableEvent();
    const exploitUsage = async () => {
      if (cameraResourceInitialized.isSignaled()) {
        await this.resume();
      } else {
        // CCA must get camera usage for completing its initialization when
        // first launched.
        await this.cameraView_.initialize();
        notifyCameraResourceReady();
        cameraResourceInitialized.signal();
      }
    };
    const releaseUsage = async () => {
      assert(cameraResourceInitialized.isSignaled());
      await this.suspend();
    };
    await ChromeHelper.getInstance().initCameraUsageMonitor(
        exploitUsage, releaseUsage);

    const startCamera = (async () => {
      await cameraResourceInitialized.wait();
      const isSuccess = await this.cameraView_.start();

      if (isSuccess) {
        const aspectRatio = this.cameraView_.getPreviewAspectRatio();
        const {width, height} = getDefaultWindowSize(aspectRatio);
        window.resizeTo(width, height);
      }

      nav.close(ViewName.SPLASH);
      nav.open(ViewName.CAMERA);

      const windowCreationTime = window['windowCreationTime'];
      this.perfLogger_.start(
          PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, windowCreationTime);
      this.perfLogger_.stop(
          PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, {hasError: !isSuccess});
      if (appWindow !== null) {
        appWindow.onAppLaunched();
      }
    })();

    const preloadImages = (async () => {
      const loadImage = (url) => new Promise((resolve, reject) => {
        const link = dom.create('link', HTMLLinkElement);
        link.rel = 'preload';
        link.as = 'image';
        link.href = url;
        link.onload = () => resolve();
        link.onerror = () =>
            reject(new Error(`Failed to preload image ${url}`));
        document.head.appendChild(link);
      });
      const results = await Promise.allSettled(
          preloadImagesList.map((name) => loadImage(`/images/${name}`)));
      const failure = results.find(({status}) => status === 'rejected');
      if (failure !== undefined) {
        error.reportError(
            ErrorType.PRELOAD_IMAGE_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(failure.reason, Error));
      }
    })();

    metrics.sendLaunchEvent({ackMigrate: false});
    return Promise.all([showWindow, startCamera, preloadImages]);
  }

  /**
   * Handles pressed keys.
   * @param {!Event} event Key press event.
   * @private
   */
  onKeyPressed_(event) {
    tooltip.hide();  // Hide shown tooltip on any keypress.
    nav.onKeyPressed(assertInstanceof(event, KeyboardEvent));
  }

  /**
   * Suspends app and hides app window.
   * @return {!Promise}
   */
  async suspend() {
    state.set(state.State.SUSPEND, true);
    await this.cameraView_.start();
    nav.open(ViewName.WARNING, WarningType.CAMERA_PAUSED);
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume() {
    state.set(state.State.SUSPEND, false);
    nav.close(ViewName.WARNING, WarningType.CAMERA_PAUSED);
  }
}

/**
 * Singleton of the App object.
 * @type {?App}
 */
let instance = null;

/**
 * Creates the App object and starts camera stream.
 */
(async () => {
  if (instance !== null) {
    return;
  }

  const perfLogger = new PerfLogger();
  const url = new URL(window.location.href);
  const intent =
      url.searchParams.get('intentId') !== null ? Intent.create(url) : null;

  state.set(state.State.INTENT, intent !== null);

  window.addEventListener('unload', () => {
    // For SWA, we don't cancel the unhandled intent here since there is no
    // guarantee that asynchronous calls in unload listener can be executed
    // properly. Therefore, we moved the logic for canceling unhandled intent to
    // Chrome (CameraAppHelper).
    if (appWindow !== null) {
      appWindow.notifyClosed();
    }
  });

  metrics.initMetrics();
  if (appWindow !== null) {
    metrics.setMetricsEnabled(false);
  }

  // Setup listener for performance events.
  perfLogger.addListener(({event, duration, perfInfo}) => {
    metrics.sendPerfEvent({event, duration, perfInfo});

    // Setup for console perf logger.
    if (state.get(state.State.PRINT_PERFORMANCE_LOGS)) {
      // eslint-disable-next-line no-console
      console.log(
          '%c%s %s ms %s', 'color: #4E4F97; font-weight: bold;',
          event.padEnd(40), duration.toFixed(0).padStart(4),
          JSON.stringify(perfInfo));
    }

    // Setup for Tast tests logger.
    if (appWindow !== null) {
      appWindow.reportPerf({event, duration, perfInfo});
    }
  });
  const states = Object.values(PerfEvent);
  states.push(state.State.TAKING);
  states.forEach((s) => {
    state.addObserver(s, (val, extras) => {
      let event = s;
      if (s === state.State.TAKING) {
        // 'taking' state indicates either taking photo or video. Skips for
        // video-taking case since we only want to collect the metrics of
        // photo-taking.
        if (state.get(Mode.VIDEO)) {
          return;
        }
        event = PerfEvent.PHOTO_TAKING;
      }

      if (val) {
        perfLogger.start(event);
      } else {
        perfLogger.stop(event, extras);
      }
    });
  });

  instance = new App({perfLogger, intent});
  await instance.start();
})();
