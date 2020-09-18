// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, promisify} from '../chrome_util.js';
import {ChromeDirectoryEntry} from '../models/chrome_file_system_entry.js';
import {Resolution} from '../type.js';
// eslint-disable-next-line no-unused-vars
import {BrowserProxy} from './browser_proxy_interface.js';

/**
 * The Chrome App implementation of the CCA's interaction with the browser.
 * @implements {BrowserProxy}
 */
class ChromeAppBrowserProxy {
  /** @override */
  async requestEnumerateDevicesPermission() {
    // It's required to run getUserMedia() successfully once before running
    // enumerateDevices(). Otherwise the deviceId and label fields would be
    // empty. See https://crbug.com/1101860 for more details.
    const doGetUserMedia = async (constraints) => {
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        stream.getTracks().forEach((track) => track.stop());
        return true;
      } catch (e) {
        return false;
      }
    };
    // Try audio stream first since it's usually much faster to open an audio
    // stream than a video stream. Note that in some form factors such as
    // Chromebox there might be no internal microphone, so the audio request
    // might fail and we would fall back to video. If no external camera
    // connected in that case, the video request might also fail and we need to
    // try again later.
    return (
        await doGetUserMedia({audio: true}) ||
        await doGetUserMedia({video: true}));
  }

  /** @override */
  async getExternalDir() {
    let volumes;
    try {
      volumes = await promisify(chrome.fileSystem.getVolumeList)();
    } catch (e) {
      console.error('Failed to get volume list', e);
      return null;
    }

    const getFileSystemRoot = async (volume) => {
      try {
        const fs = await promisify(chrome.fileSystem.requestFileSystem)(volume);
        return fs === null ? null : fs.root;
      } catch (e) {
        console.error('Failed to request file system', e);
        return null;
      }
    };

    for (const volume of volumes) {
      if (!volume.volumeId.includes('downloads:MyFiles')) {
        continue;
      }
      const root = await getFileSystemRoot(volume);
      if (root === null) {
        continue;
      }

      const rootEntry = new ChromeDirectoryEntry(root);
      const entries = await rootEntry.getDirectories();
      return entries.find((entry) => entry.name === 'Downloads') || null;
    }
    return null;
  }

  /** @override */
  localStorageGet(keys) {
    return promisify(chrome.storage.local.get.bind(chrome.storage.local))(keys);
  }

  /** @override */
  localStorageSet(items) {
    return promisify(chrome.storage.local.set.bind(chrome.storage.local))(
        items);
  }

  /** @override */
  localStorageRemove(items) {
    return promisify(chrome.storage.local.remove.bind(chrome.storage.local))(
        items);
  }

  /** @override */
  async getBoard() {
    const values = await promisify(chrome.chromeosInfoPrivate.get)(['board']);
    return values['board'];
  }

  /** @override */
  getI18nMessage(name, ...substitutions) {
    return chrome.i18n.getMessage(name, substitutions);
  }

  /** @override */
  addOnLockListener(callback) {
    chrome.idle.onStateChanged.addListener((newState) => {
      callback(newState === 'locked');
    });
  }

  /** @override */
  isMetricsAndCrashReportingEnabled() {
    return promisify(chrome.metricsPrivate.getIsCrashReportingEnabled)();
  }

  /** @override */
  async openGallery(file) {
    const id = 'jhdjimmaggjajfjphpljagpgkidjilnj|web|open';
    try {
      const result = await promisify(chrome.fileManagerPrivate.executeTask)(
          id, [file.getRawEntry()]);
      if (result !== 'message_sent') {
        console.warn('Unable to open picture: ' + result);
      }
    } catch (e) {
      console.warn('Unable to open picture', e);
      return;
    }
  }

  /** @override */
  openInspector(type) {
    chrome.fileManagerPrivate.openInspector(type);
  }

  /** @override */
  getAppVersion() {
    return chrome.runtime.getManifest().version;
  }

  /** @override */
  addOnMessageExternalListener(listener) {
    chrome.runtime.onMessageExternal.addListener(listener);
  }

  /** @override */
  addOnConnectExternalListener(listener) {
    chrome.runtime.onConnectExternal.addListener(listener);
  }

  /** @override */
  addDummyHistoryIfNotAvailable() {
    // Since GA will use history.length to generate hash but it is not available
    // in platform apps, set it to 1 manually.
    window.history.length = 1;
  }

  /** @override */
  isMp4RecordingEnabled() {
    return true;
  }

  /** @override */
  getBackgroundOps() {
    assert(window['backgroundOps'] !== undefined);
    return window['backgroundOps'];
  }

  /** @override */
  isFullscreenOrMaximized() {
    return chrome.app.window.current().outerBounds.width >= screen.width ||
        chrome.app.window.current().outerBounds.height >= screen.height;
  }

  /** @override */
  async fitWindow() {
    const appWindow = chrome.app.window.current();

    /**
     * Get a preferred window size which can fit in current screen.
     * @return {!Resolution} Preferred window size.
     */
    const getPreferredWindowSize = () => {
      const inner = appWindow.innerBounds;
      const outer = appWindow.outerBounds;

      const predefinedWidth = inner.minWidth;
      const availableWidth = screen.availWidth;

      const topBarHeight = outer.height - inner.height;
      const fixedRatioMaxWidth =
          Math.floor((screen.availHeight - topBarHeight) * 16 / 9);

      let preferredWidth =
          Math.min(predefinedWidth, availableWidth, fixedRatioMaxWidth);
      preferredWidth -= preferredWidth % 16;
      const preferredHeight = preferredWidth * 9 / 16;

      return new Resolution(preferredWidth, preferredHeight);
    };

    const {width, height} = getPreferredWindowSize();

    return new Promise((resolve) => {
      const inner = appWindow.innerBounds;
      if (inner.width === width && inner.height === height) {
        resolve();
        return;
      }

      const listener = () => {
        appWindow.onBoundsChanged.removeListener(listener);
        resolve();
      };
      appWindow.onBoundsChanged.addListener(listener);

      Object.assign(inner, {width, height, minWidth: width, minHeight: height});
    });
  }

  /** @override */
  showWindow() {
    chrome.app.window.current().show();
  }

  /** @override */
  hideWindow() {
    chrome.app.window.current().hide();
  }

  /** @override */
  isMinimized() {
    return chrome.app.window.current().isMinimized();
  }

  /** @override */
  addOnMinimizedListener(listener) {
    chrome.app.window.current().onMinimized.addListener(listener);
  }

  /** @override */
  openFeedback() {
    const data = {
      'categoryTag': 'chromeos-camera-app',
      'requestFeedback': true,
      'feedbackInfo': {
        'descriptionPlaceholder':
            this.getI18nMessage('feedback_description_placeholder'),
        'systemInformation': [
          {key: 'APP ID', value: chrome.runtime.id},
          {key: 'APP VERSION', value: chrome.runtime.getManifest().version},
        ],
      },
    };
    const id = 'gfdkimpbcpahaombhbimeihdjnejgicl';  // Feedback extension id.
    chrome.runtime.sendMessage(id, data);
  }
}

export const browserProxy = new ChromeAppBrowserProxy();
