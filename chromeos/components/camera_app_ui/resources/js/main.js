// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  BackgroundOps,  // eslint-disable-line no-unused-vars
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
import * as nav from './nav.js';
import {PerfEvent} from './perf.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {Mode, ViewName} from './type.js';
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
import {Warning} from './views/warning.js';

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
    await this.cameraView_.initialize();

    try {
      await filesystem.initialize();

      const promptMigrate = async () => {
        // Prompt to migrate pictures if needed.
        const message = browserProxy.getI18nMessage('migrate_pictures_msg');
        const acked = await nav.open(
            ViewName.MESSAGE_DIALOG, {message, cancellable: false});
        if (!acked) {
          throw new Error('no-migrate');
        }
      };
      // Migrate pictures might take some time. Since it won't affect other
      // camera functions, we don't await here to avoid harming UX.
      filesystem.checkMigration(promptMigrate).then((ackMigrate) => {
        metrics.sendLaunchEvent({ackMigrate});
      });

      const externalDir = filesystem.getExternalDirectory();
      assert(externalDir !== null);
      this.galleryButton_.initialize(externalDir);
    } catch (error) {
      console.error(error);
      if (error && error.message === 'no-migrate') {
        window.close();
        return;
      }
      nav.open(ViewName.WARNING, 'filesystem-failure');
    }

    const showWindow = (async () => {
      await browserProxy.fitWindow();
      browserProxy.showWindow();
      this.backgroundOps_.notifyActivation();
    })();
    const startCamera = (async () => {
      const isSuccess = await this.cameraView_.start();
      nav.close(ViewName.SPLASH);
      nav.open(ViewName.CAMERA);
      this.backgroundOps_.getPerfLogger().stopLaunch({hasError: !isSuccess});
    })();

    return Promise.all([showWindow, startCamera]);
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
    browserProxy.hideWindow();
    this.backgroundOps_.notifySuspension();
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume() {
    state.set(state.State.SUSPEND, false);
    browserProxy.showWindow();
    this.backgroundOps_.notifyActivation();
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
  const bgOps = browserProxy.getBackgroundOps();

  const testErrorCallback = bgOps.getTestingErrorCallback();
  metrics.initMetrics();
  if (testErrorCallback !== null) {
    metrics.setMetricsEnabled(false);
  }

  // TODO(crbug.com/1082585): Initializes it before any other javascript loaded.
  error.initialize(testErrorCallback);

  const perfLogger = bgOps.getPerfLogger();

  // Setup listener for performance events.
  perfLogger.addListener((event, duration, extras) => {
    metrics.sendPerfEvent({event, duration, extras});
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

  // Setup for console perf logger.
  perfLogger.addListener((event, duration, extras) => {
    if (state.get(state.State.PRINT_PERFORMANCE_LOGS)) {
      // eslint-disable-next-line no-console
      console.log(
          '%c%s %s ms %s', 'color: #4E4F97; font-weight: bold;',
          event.padEnd(40), duration.toFixed(0).padStart(4),
          JSON.stringify(extras));
    }
  });

  instance = new App(
      /** @type {!BackgroundOps} */ (bgOps));
  await instance.start();
})();
