// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  BackgroundOps,  // eslint-disable-line no-unused-vars
  ForegroundOps,  // eslint-disable-line no-unused-vars
} from './background_ops.js';
import {browserProxy} from './browser_proxy/browser_proxy.js';
// eslint-disable-next-line no-unused-vars
import {TestingErrorCallback} from './error.js';
import {Intent} from './intent.js';
import {initMetrics, setMetricsEnabled} from './metrics.js';
import {PerfEvent, PerfLogger} from './perf.js';

/**
 * Fixed minimum width of the window inner-bounds in pixels.
 * @type {number}
 */
const MIN_WIDTH = 864;

/**
 * Initial apsect ratio of the window inner-bounds.
 * @type {number}
 */
const INITIAL_ASPECT_RATIO = 1.7777777777;

/**
 * Top bar color of the window.
 * @type {string}
 */
const TOPBAR_COLOR = '#000000';

/**
 * The origin of the test app used in Tast.
 * @type {string}
 */
const TEST_API_ORIGIN = 'chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj';

/**
 * It's used in test to catch the perf event before the creation of app window
 * for time measurement before launch.
 * @type {?PerfLogger}
 */
let perfLoggerForTesting = null;

/**
 * Background object for handling launch event.
 * @type {?Background}
 */
let background = null;

/**
 * State of CCAWindow.
 * @enum {string}
 */
const WindowState = {
  UNINIT: 'uninitialized',
  LAUNCHING: 'launching',
  ACTIVE: 'active',
  SUSPENDING: 'suspending',
  SUSPENDED: 'suspended',
  RESUMING: 'resuming',
  CLOSING: 'closing',
  CLOSED: 'closed',
};

/**
 * Callbacks called when specific window events happened.
 * @typedef {{
 *   onActive: function(!CCAWindow),
 *   onSuspended: function(!CCAWindow),
 *   onClosed: function(!CCAWindow),
 * }}
 */
let WindowEventCallbacks;  // eslint-disable-line no-unused-vars

/**
 * Callbacks called when specific window events happened in test run.
 * onCreated()/onClosed() is called when AppWindow created/closed with window
 * url. onError() is called with error happening in AppWindow.
 * @typedef {{
 *   onCreated: function(string),
 *   onClosed: function(string),
 *   onError: !TestingErrorCallback,
 * }}
 */
let WindowTestEventCallbacks;  // eslint-disable-line no-unused-vars

/**
 * Set in test mode for notifying test events.
 * @type {?WindowTestEventCallbacks}
 */
let windowTestEventCallbacks = null;

/**
 * Wrapper of AppWindow for tracking its state.
 * @implements {BackgroundOps}
 */
class CCAWindow {
  /**
   * @param {!WindowEventCallbacks} callbacks
   * @param {?WindowTestEventCallbacks} testingCallbacks
   * @param {?PerfLogger} perfLogger The logger for perf events. If it
   *     is null, we will create a new one for the window.
   * @param {?Intent=} intent Intent to be handled by the app window.
   *     Set to null for app window not launching from intent.
   */
  constructor(callbacks, testingCallbacks, perfLogger, intent = null) {
    /**
     * @type {!WindowEventCallbacks}
     */
    this.callbacks_ = callbacks;

    /**
     * @type {?WindowTestEventCallbacks}
     */
    this.testingCallbacks_ = testingCallbacks;

    /**
     * @type {?Intent}
     * @private
     */
    this.intent_ = intent;

    /**
     * @type {!PerfLogger}
     * @private
     */
    this.perfLogger_ = perfLogger || new PerfLogger();

    /**
     * @type {?chrome.app.window.AppWindow}
     * @private
     */
    this.appWindow_ = null;

    /**
     * @type {?ForegroundOps}
     * @private
     */
    this.foregroundOps_ = null;

    /**
     * @type {!WindowState}
     * @private
     */
    this.state_ = WindowState.UNINIT;
  }

  /**
   * Gets state of the window.
   * @return {!WindowState}
   */
  get state() {
    return this.state_;
  }

  /**
   * Creates app window and launches app.
   * @return {!Promise}
   */
  async launch() {
    // Disables the metrics sending if it is testing window.
    if (this.testingCallbacks_ !== null) {
      await setMetricsEnabled(false);
    }

    this.state_ = WindowState.LAUNCHING;

    // The height will be later calculated to match video aspect ratio once the
    // stream is available.
    const initialHeight = Math.round(MIN_WIDTH / INITIAL_ASPECT_RATIO);

    const windowId =
        this.intent_ !== null ? `main-${this.intent_.intentId}` : 'main';
    const windowUrl = 'views/main.html' +
        (this.intent_ !== null ? this.intent_.url.search : '');

    chrome.app.window.create(
        windowUrl, {
          id: windowId,
          frame: {color: TOPBAR_COLOR},
          hidden: true,  // Will be shown from main.js once loaded.
          innerBounds: {
            width: MIN_WIDTH,
            height: initialHeight,
            minWidth: MIN_WIDTH,
            left: Math.round((window.screen.availWidth - MIN_WIDTH) / 2),
            top: Math.round((window.screen.availHeight - initialHeight) / 2),
          },
        },
        (appWindow) => {
          this.perfLogger_.start(PerfEvent.LAUNCHING_FROM_WINDOW_CREATION);
          this.appWindow_ = appWindow;
          this.appWindow_.onClosed.addListener(() => {
            browserProxy.localStorageSet({maximized: appWindow.isMaximized()});
            browserProxy.localStorageSet(
                {fullscreen: appWindow.isFullscreen()});
            this.state_ = WindowState.CLOSED;
            if (this.intent_ !== null && !this.intent_.done) {
              this.intent_.cancel();
            }
            this.callbacks_.onClosed(this);
            if (this.testingCallbacks_ !== null) {
              this.testingCallbacks_.onClosed(windowUrl);
            }
          });
          appWindow.contentWindow['backgroundOps'] = this;
          if (this.testingCallbacks_ !== null) {
            this.testingCallbacks_.onCreated(windowUrl);
          }
        });
  }

  /**
   * @override
   */
  bindForegroundOps(ops) {
    this.foregroundOps_ = ops;
  }

  /**
   * @override
   */
  getIntent() {
    return this.intent_;
  }

  /**
   * @override
   */
  notifyActivation() {
    this.state_ = WindowState.ACTIVE;
    // For intent only requiring open camera with specific mode without
    // returning the capture result, called onIntentHandled() right
    // after app successfully launched.
    if (this.intent_ !== null && !this.intent_.shouldHandleResult) {
      this.intent_.finish();
    }
    this.callbacks_.onActive(this);
  }

  /**
   * @override
   */
  notifySuspension() {
    this.state_ = WindowState.SUSPENDED;
    this.callbacks_.onSuspended(this);
  }

  /**
   * @override
   */
  getPerfLogger() {
    return this.perfLogger_;
  }

  /**
   * @override
   */
  getTestingErrorCallback() {
    return this.testingCallbacks_ ? this.testingCallbacks_.onError : null;
  }

  /**
   * Suspends the app window.
   */
  suspend() {
    if (this.state_ === WindowState.LAUNCHING) {
      console.error('Call suspend() while window is still launching.');
      return;
    }
    this.state_ = WindowState.SUSPENDING;
    this.foregroundOps_.suspend();
  }

  /**
   * Resumes the app window.
   */
  resume() {
    this.state_ = WindowState.RESUMING;
    this.foregroundOps_.resume();
  }

  /**
   * Closes the app window.
   */
  close() {
    this.state_ = WindowState.CLOSING;
    this.appWindow_.close();
  }

  /**
   * Minimize or restore the app window.
   */
  minimizeOrRestore() {
    if (this.appWindow_.isMinimized()) {
      this.appWindow_.restore();
      this.appWindow_.focus();
    } else {
      this.appWindow_.minimize();
    }
  }
}

/**
 * Launch event handler runs in background.
 */
class Background {
  /**
   * @public
   */
  constructor() {
    /**
     * Launch window handles launch event triggered from app launcher.
     * @type {?CCAWindow}
     * @private
     */
    this.launcherWindow_ = null;

    /**
     * Intent window handles launch event triggered from ARC++ intent.
     * @type {?CCAWindow}
     * @private
     */
    this.intentWindow_ = null;

    /**
     * The pending intent arrived when foreground window is busy.
     * @type {?Intent}
     */
    this.pendingIntent_ = null;

    // By default, we enable the metrics sending on background page and will
    // turn it off when launching testing windows.
    initMetrics();
  }

  /**
   * Checks and logs any violation of background transition logic.
   * @param {boolean} assertion Condition to be asserted.
   * @param {string|function(): string} message Logged message.
   * @private
   */
  assert_(assertion, message) {
    if (!assertion) {
      console.error(typeof message === 'string' ? message : message());
    }
    // TODO(inker): Cleans up states and starts over after any violation.
  }

  /**
   * Processes the pending intent.
   * @private
   */
  processPendingIntent_() {
    if (!this.pendingIntent_) {
      console.error('Call processPendingIntent_() without intent present.');
      return;
    }
    this.intentWindow_ = this.createIntentWindow_(this.pendingIntent_);
    this.pendingIntent_ = null;
    this.intentWindow_.launch();
  }

  /**
   * Returns a Window object handling launch event triggered from app launcher.
   * @return {!CCAWindow}
   * @private
   */
  createLauncherWindow_() {
    const onActive = (wnd) => {
      this.assert_(wnd === this.launcherWindow_, 'Wrong active launch window.');
      this.assert_(
          !this.intentWindow_,
          'Launch window is active while handling intent window.');
      if (this.pendingIntent_ !== null) {
        wnd.suspend();
      }
    };
    const onSuspended = (wnd) => {
      this.assert_(
          wnd === this.launcherWindow_, 'Wrong suspended launch window.');
      this.assert_(
          !this.intentWindow_,
          'Launch window is suspended while handling intent window.');
      if (this.pendingIntent_ === null) {
        this.assert_(
            false, 'Launch window is not suspended by some pending intent');
        wnd.resume();
        return;
      }
      this.processPendingIntent_();
    };
    const onClosed = (wnd) => {
      this.assert_(wnd === this.launcherWindow_, 'Wrong closed launch window.');
      this.launcherWindow_ = null;
      if (this.pendingIntent_ !== null) {
        this.processPendingIntent_();
      }
    };

    const wnd = new CCAWindow(
        {onActive, onSuspended, onClosed}, windowTestEventCallbacks,
        perfLoggerForTesting);
    windowTestEventCallbacks = null;
    perfLoggerForTesting = null;
    return wnd;
  }

  /**
   * Returns a Window object handling launch event triggered from ARC++ intent.
   * @param {!Intent} intent Intent forwarding from ARC++.
   * @return {!CCAWindow}
   * @private
   */
  createIntentWindow_(intent) {
    const onActive = (wnd) => {
      this.assert_(wnd === this.intentWindow_, 'Wrong active intent window.');
      this.assert_(
          !this.launcherWindow_ ||
              this.launcherWindow_.state === WindowState.SUSPENDED,
          () => `Launch window is ${
              this.launcherWindow_.state} when intent window is active.`);
      if (this.pendingIntent_) {
        wnd.close();
      }
    };
    const onSuspended = (wnd) => {
      this.assert_(
          wnd === this.intentWindow_, 'Wrong suspended intent window.');
      this.assert_(false, 'Intent window should not be suspended.');
    };
    const onClosed = (wnd) => {
      this.assert_(wnd === this.intentWindow_, 'Wrong closed intent window.');
      this.assert_(
          !this.launcherWindow_ ||
              this.launcherWindow_.state === WindowState.SUSPENDED,
          () => `Launch window is ${
              this.launcherWindow_.state} when intent window is closed.`);
      this.intentWindow_ = null;
      if (this.pendingIntent_) {
        this.processPendingIntent_();
      } else if (this.launcherWindow_) {
        this.launcherWindow_.resume();
      }
    };

    const wnd = new CCAWindow(
        {onActive, onSuspended, onClosed}, windowTestEventCallbacks,
        perfLoggerForTesting, intent);
    windowTestEventCallbacks = null;
    perfLoggerForTesting = null;
    return wnd;
  }

  /**
   * Handles launch event triggered from app launcher.
   */
  launchApp() {
    if (this.launcherWindow_ || this.intentWindow_) {
      const activeWindow = [this.launcherWindow_, this.intentWindow_].find(
          (wnd) => wnd !== null && wnd.state_ === WindowState.ACTIVE);
      if (activeWindow !== undefined) {
        activeWindow.minimizeOrRestore();
      }
      return;
    }
    this.assert_(
        !this.pendingIntent_,
        'Pending intent is not processed when launch new window.');
    this.launcherWindow_ = this.createLauncherWindow_();
    this.launcherWindow_.launch();
  }

  /**
   * Closes the existing pending intent and replaces it with a new incoming
   * intent.
   * @param {!Intent} intent New incoming intent.
   * @private
   */
  replacePendingIntent_(intent) {
    if (this.pendingIntent_) {
      this.pendingIntent_.cancel();
    }
    this.pendingIntent_ = intent;
  }

  /**
   * Handles launch event triggered from ARC++ intent.
   * @param {!Intent} intent Intent forwarding from ARC++.
   */
  launchIntent(intent) {
    if (this.intentWindow_) {
      switch (this.intentWindow_.state) {
        case WindowState.LAUNCHING:
        case WindowState.CLOSING:
          this.replacePendingIntent_(intent);
          break;
        case WindowState.ACTIVE:
          this.replacePendingIntent_(intent);
          this.intentWindow_.close();
          break;
        default:
          this.assert_(
              false,
              `Intent window is ${
                  this.intentWindow_.state} when launch new intent window.`);
      }
    } else if (this.launcherWindow_) {
      switch (this.launcherWindow_.state) {
        case WindowState.LAUNCHING:
        case WindowState.SUSPENDING:
        case WindowState.RESUMING:
        case WindowState.CLOSING:
          this.replacePendingIntent_(intent);
          break;
        case WindowState.ACTIVE:
          this.assert_(
              !this.pendingIntent_,
              'Pending intent is not processed when launch window is active.');
          this.replacePendingIntent_(intent);
          this.launcherWindow_.suspend();
          break;
        default:
          this.assert_(
              false,
              `Launch window is ${
                  this.launcherWindow_.state} when launch new intent window.`);
      }
    } else {
      this.intentWindow_ = this.createIntentWindow_(intent);
      this.intentWindow_.launch();
    }
  }
}

/**
 * Handles connection from the test extension used in Tast.
 * @param {!Port} port The port that used to do two-way communication.
 */
function handleExternalConnectionFromTest(port) {
  if (port.sender.origin !== TEST_API_ORIGIN) {
    console.warn(`Unknown sender id: ${port.sender.id}`);
    return;
  }
  switch (port.name) {
    case 'SET_PERF_CONNECTION':
      port.onMessage.addListener((event) => {
        if (perfLoggerForTesting === null) {
          perfLoggerForTesting = new PerfLogger();

          perfLoggerForTesting.addListener((event, duration, extras) => {
            port.postMessage({event, duration, extras});
          });
        }

        const {name} = event;
        if (name !== PerfEvent.LAUNCHING_FROM_LAUNCH_APP_COLD &&
            name !== PerfEvent.LAUNCHING_FROM_LAUNCH_APP_WARM) {
          console.warn(`Unknown event name from test: ${name}`);
          return;
        }
        perfLoggerForTesting.start(name);
      });
      return;
    case 'SET_TEST_CONNECTION':
      // TODO(crbug.com/1082133): Documents or adds typing to the test
      // connection port message.
      windowTestEventCallbacks = {
        onCreated: (windowUrl) => {
          port.postMessage({name: 'connect', windowUrl});
        },
        onClosed: (windowUrl) => {
          port.postMessage({name: 'disconnect', windowUrl});
        },
        onError: (errorInfo) => port.postMessage({name: 'error', errorInfo}),
      };
      return;
    default:
      console.warn(`Unknown port name: ${port.name}`);
  }
}

chrome.app.runtime.onLaunched.addListener((launchData) => {
  if (!background) {
    background = new Background();
  }
  try {
    const /** (string|undefined) */ url = launchData['url'];
    if (url !== undefined) {
      const intent = Intent.create(new URL(url));
      background.launchIntent(intent);
    } else {
      background.launchApp();
    }
  } catch (e) {
    console.error(e.stack);
  }
});

browserProxy.addOnConnectExternalListener(handleExternalConnectionFromTest);
