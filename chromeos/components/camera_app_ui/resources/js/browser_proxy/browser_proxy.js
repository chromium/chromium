// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {promisify} from '../chrome_util.js';
import {ChromeDirectoryEntry} from '../models/chrome_file_system_entry.js';
import {getMaybeLazyDirectory} from '../models/lazy_directory_entry.js';
import {UntrustedOrigin} from '../type.js';

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
  async getCameraDirectory() {
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

      const myFilesDir = new ChromeDirectoryEntry(root);
      return getMaybeLazyDirectory(myFilesDir, 'Camera');
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
  localStorageClear() {
    return promisify(chrome.storage.local.clear.bind(chrome.storage.local))();
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
  getTextDirection() {
    return this.getI18nMessage('@@bidi_dir');
  }

  /** @override */
  shouldAddFakeHistory() {
    return true;
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

  /** @override */
  async initCameraUsageMonitor(exploitUsage, releaseUsage) {
    // For platform app, since the multi-window behavior is handled in
    // background page, we can assume when the new CCA instance is launched,
    // the camera usage has been already released by the previous CCA instance
    // and we can safely use the camera.
    await exploitUsage();
  }

  /** @override */
  setupUnloadListener(listener) {
    // Platform app should use chrome.app.window.AppWindow onClosed event
    // listener in background page instead of window unload event listener.
  }

  /** @override */
  async setLaunchingFromWindowCreationStartTime(callback) {
    // For platform app, the start time of window creation is recorded by
    // background page so we don't need to trigger it here.
  }

  /** @override */
  getUntrustedOrigin() {
    return UntrustedOrigin.CHROME_EXTENSION;
  }

  /** @override */
  setBeforeUnloadListenerEnabled(enabled) {
    // Do nothing since beforeunload event is unavailable for platform apps.
  }
}

export const browserProxy = new ChromeAppBrowserProxy();
