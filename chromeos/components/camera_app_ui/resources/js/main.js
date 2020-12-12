// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  AppWindow,  // eslint-disable-line no-unused-vars
  DEFAULT_PREVIEW_16X9_WINDOW_SIZE,
  DEFAULT_PREVIEW_4X3_WINDOW_SIZE,
} from './app_window.js';
import {
  BackgroundOps,  // eslint-disable-line no-unused-vars
  createFakeBackgroundOps,
  ForegroundOps,  // eslint-disable-line no-unused-vars
} from './background_ops.js';
import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assert, assertInstanceof} from './chrome_util.js';
import {
  PhotoConstraintsPreferrer,
  VideoConstraintsPreferrer,
} from './device/constraints_preferrer.js';
import {DeviceInfoUpdater} from './device/device_info_updater.js';
import * as dom from './dom.js';
import * as error from './error.js';
import {GalleryButton} from './gallerybutton.js';
import * as metrics from './metrics.js';
import * as filesystem from './models/file_system.js';
import {notifyCameraResourceReady} from './mojo/device_operator.js';
import * as nav from './nav.js';
import {preloadImagesList} from './preload_images.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {ErrorLevel, ErrorType, Mode, PerfEvent, ViewName} from './type.js';
import * as util from './util.js';
import {Camera} from './views/camera.js';
import {CameraIntent} from './views/camera_intent.js';
import {Dialog} from './views/dialog.js';
import {
  BaseSettings,
  PrimarySettings,
  ResolutionSettings,
} from './views/settings.js';
import {View} from './views/view.js';
import {Warning, WarningType} from './views/warning.js';
import {WaitableEvent} from './waitable_event.js';
import {windowController} from './window_controller/window_controller.js';

/**
 * The app window instance which is used for communication with Tast tests. For
 * non-test sessions or test sessions but using the legacy communication
 * solution (chrome.runtime), it should be null.
 * @type {?AppWindow}
 */
const appWindow = window['appWindow'];

/**
 * Creates the Camera App main object.
 * @implements {ForegroundOps}
 */
export class App {
  /**
   * @param {!BackgroundOps} backgroundOps
   */
  constructor(backgroundOps) {
    /**
     * @type {!BackgroundOps}
     * @private
     */
    this.backgroundOps_ = backgroundOps;

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
      const intent = this.backgroundOps_.getIntent();
      const perfLogger = this.backgroundOps_.getPerfLogger();
      if (intent !== null && intent.shouldHandleResult) {
        state.set(state.State.SHOULD_HANDLE_INTENT_RESULT, true);
        return new CameraIntent(
            intent, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, perfLogger);
      } else {
        const mode = intent !== null ? intent.mode : Mode.PHOTO;
        return new Camera(
            this.galleryButton_, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, mode, perfLogger);
      }
    })();

    document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

    document.title = browserProxy.getI18nMessage('name');
    util.setupI18nElements(document.body);
    this.setupToggles_();
    this.setupSettingEffect_();

    const resolutionSettings = new ResolutionSettings(
        this.infoUpdater_, this.photoPreferrer_, this.videoPreferrer_);

    // Set up views navigation by their DOM z-order.
    nav.setup([
      this.cameraView_,
      new PrimarySettings(),
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
    this.backgroundOps_.bindForegroundOps(this);
    this.backgroundOps_.bindAppWindow(appWindow);
  }

  /**
   * Sets up toggles (checkbox and radio) by data attributes.
   * @private
   */
  setupToggles_() {
    browserProxy.localStorageGet({expert: false})
        .then((values) => state.set(state.State.EXPERT, values['expert']));
    dom.getAll('input', HTMLInputElement).forEach((element) => {
      element.addEventListener('keypress', (event) => {
        const e = assertInstanceof(event, KeyboardEvent);
        if (util.getShortcutIdentifier(e) === 'Enter') {
          element.click();
        }
      });

      const payload = (element) =>
          ({[element.dataset['key']]: element.checked});
      const save = (element) => {
        if (element.dataset['key'] !== undefined) {
          browserProxy.localStorageSet(payload(element));
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
            document.querySelectorAll(grouped).forEach(
                (radio) =>
                    radio.dispatchEvent(new Event('change')) && save(radio));
          }
        }
      });
      if (element.dataset['key'] !== undefined) {
        // Restore the previously saved state on startup.
        browserProxy.localStorageGet(payload(element))
            .then(
                (values) => util.toggleChecked(
                    element, values[element.dataset['key']]));
      }
    });
  }

  /**
   * Sets up inkdrop effect for settings view.
   * @private
   */
  setupSettingEffect_() {
    dom.getAll('button.menu-item, label.menu-item', HTMLElement)
        .forEach((el) => util.setInkdropEffect(el));
  }

  /**
   * Starts the app by loading the model and opening the camera-view.
   * @return {!Promise}
   */
  async start() {
    document.documentElement.dir = browserProxy.getTextDirection();
    try {
      await filesystem.initialize();
      const cameraDir = filesystem.getCameraDirectory();
      assert(cameraDir !== null);
      this.galleryButton_.initialize(cameraDir);
    } catch (error) {
      console.error(error);
      nav.open(ViewName.WARNING, WarningType.FILESYSTEM_FAILURE);
    }

    const showWindow = (async () => {
      windowController.enable();
      this.backgroundOps_.notifyActivation();
      // For intent only requiring open camera with specific mode without
      // returning the capture result, called onIntentHandled() right
      // after app successfully launched.
      const intent = this.backgroundOps_.getIntent();
      if (intent !== null && !intent.shouldHandleResult) {
        intent.finish();
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
    await browserProxy.initCameraUsageMonitor(exploitUsage, releaseUsage);

    const startCamera = (async () => {
      await cameraResourceInitialized.wait();
      const isSuccess = await this.cameraView_.start();

      if (isSuccess) {
        const aspectRatio = this.cameraView_.getPreviewAspectRatio();
        if (Math.abs(4 / 3 - aspectRatio) < Math.abs(16 / 9 - aspectRatio)) {
          window.resizeTo(...DEFAULT_PREVIEW_4X3_WINDOW_SIZE);
        } else {
          window.resizeTo(...DEFAULT_PREVIEW_16X9_WINDOW_SIZE);
        }
      }

      nav.close(ViewName.SPLASH);
      nav.open(ViewName.CAMERA);
      await browserProxy.setLaunchingFromWindowCreationStartTime(async () => {
        const windowCreationTime = window['windowCreationTime'];
        this.backgroundOps_.getPerfLogger().start(
            PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, windowCreationTime);
      });
      this.backgroundOps_.getPerfLogger().stop(
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
        link.onerror = (e) => reject(e.reason);
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
    windowController.disable();
    this.backgroundOps_.notifySuspension();
    nav.open(ViewName.WARNING, WarningType.CAMERA_BEING_USED);
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume() {
    state.set(state.State.SUSPEND, false);
    windowController.enable();
    this.backgroundOps_.notifyActivation();
    nav.close(ViewName.WARNING, WarningType.CAMERA_BEING_USED);
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

  let bgOps;
  if (window['backgroundOps'] !== undefined) {
    bgOps = window['backgroundOps'];
  } else {
    // TODO(crbug.com/980846): Refactor after migrating to SWA since there is no
    // background page for SWA.
    bgOps = createFakeBackgroundOps();
  }

  browserProxy.setupUnloadListener(() => {
    // For SWA, we don't cancel the unhandled intent here since there is no
    // guarantee that asynchronous calls in unload listener can be executed
    // properly. Therefore, we moved the logic for canceling unhandled intent to
    // Chrome (CameraAppHelper).
    if (appWindow !== null) {
      appWindow.notifyClosed();
    }
  });

  const testErrorCallback = bgOps.getTestingErrorCallback();
  metrics.initMetrics();
  if (testErrorCallback !== null || appWindow !== null) {
    metrics.setMetricsEnabled(false);
  }

  // TODO(crbug.com/1082585): Initializes it before any other javascript loaded.
  error.initialize(testErrorCallback);

  const perfLogger = bgOps.getPerfLogger();

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

  instance = new App(
      /** @type {!BackgroundOps} */ (bgOps));
  await instance.start();
})();
