// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeVoxSubpageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestChromeVoxSubpageBrowserProxy extends TestBrowserProxy
    implements ChromeVoxSubpageBrowserProxy {
  constructor() {
    super([
      'getAllTtsVoiceData',
      'refreshTtsVoices',
      'getDisplayNameForLocale',
      'getApplicationLocale',
      'addDeviceAddedListener',
      'removeDeviceAddedListener',
      'addDeviceChangedListener',
      'removeDeviceChangedListener',
      'addDeviceRemovedListener',
      'removeDeviceRemovedListener',
      'addPairingListener',
      'removePairingListener',
      'startDiscovery',
      'stopDiscovery',
      'updateBluetoothBrailleDisplayAddress',
    ]);
  }

  getAllTtsVoiceData(): void {
    const voices = [
      {
        name: 'Chrome OS US English',
        remote: false,
        extensionId: 'gjjabgpgjpampikjhjpfhneeoapjbjaf',
      },
      {
        name: 'Chrome OS हिन्दी',
        remote: false,
        extensionId: 'gjjabgpgjpampikjhjpfhneeoapjbjaf',
      },
      {
        name: 'default-coolnet',
        remote: false,
        extensionId: 'abcdefghijklmnop',
      },
      {
        name: 'bnm',
        remote: false,
        extensionId: 'abcdefghijklmnop',
      },
      {
        name: 'bnx',
        remote: true,
        extensionId: 'abcdefghijklmnop',
      },
      {
        name: 'eSpeak Turkish',
        remote: false,
        extensionId: 'dakbfdmgjiabojdgbiljlhgjbokobjpg',
      },
    ];
    webUIListenerCallback('all-voice-data-updated', voices);
  }

  refreshTtsVoices(): void {
    this.methodCalled('refreshTtsVoices');
  }

  getDisplayNameForLocale(locale: string): Promise<string> {
    this.methodCalled('getDisplayNameForLocale');
    return Promise.resolve(locale);
  }

  getApplicationLocale(): Promise<string> {
    this.methodCalled('getApplicationLocale');
    return Promise.resolve('');
  }

  addDeviceAddedListener(): void {
    this.methodCalled('addDeviceAddedListener');
  }
  removeDeviceAddedListener(): void {
    this.methodCalled('removeDeviceAddedListener');
  }
  addDeviceChangedListener(): void {
    this.methodCalled('addDeviceChangedListener');
  }
  removeDeviceChangedListener(): void {
    this.methodCalled('removeDeviceChangedListener');
  }
  addDeviceRemovedListener(): void {
    this.methodCalled('addDeviceRemovedListener');
  }
  removeDeviceRemovedListener(): void {
    this.methodCalled('removeDeviceRemovedListener');
  }
  addPairingListener(): void {
    this.methodCalled('addPairingListener');
  }
  removePairingListener(): void {
    this.methodCalled('removePairingListener');
  }
  startDiscovery(): void {
    this.methodCalled('startDiscovery');
  }
  stopDiscovery(): void {
    this.methodCalled('stopDiscovery');
  }
  updateBluetoothBrailleDisplayAddress(): void {
    this.methodCalled('updateBluetoothBrailleDisplayAddress');
  }
}
