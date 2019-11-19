/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * An object containing information about the Chromebook's latest Enrollment or
 * DeviceSync call to the CryptAuth server.
 * @typedef {{
 *   lastSuccessTime: number,
 *   nextRefreshTime: number,
 *   recoveringFromFailure: boolean,
 *   operationInProgress: boolean,
 * }}
 */
let SyncState;

/**
 * @typedef {{
 *   userPresent: number,
 *   secureScreenLock: number,
 *   trustAgent: number,
 *}}
 */
let RemoteState;

/**
 * An object containing data for devices returned by CryptAuth DeviceSync.
 * @typedef {{
 *   publicKey: string,
 *   publicKeyTruncated: string,
 *   friendlyDeviceName: string,
 *   noPiiName: string,
 *   unlockKey: boolean,
 *   hasMobileHotspot: boolean,
 *   connectionStatus: string,
 *   featureStates: string,
 *   remoteState: (!RemoteState|undefined),
 *   isArcPlusPlusEnrollment: (boolean|undefined),
 *   isPixelPhone: (boolean|undefined),
 *   bluetoothAddress: (string|undefined),
 * }}
 */
let RemoteDevice;

const ProximityAuth = {
  cryptauthController_: null,
  remoteDevicesController_: null,
  findEligibleDevicesController_: null,

  /**
   * Initializes all UI elements of the ProximityAuth debug page.
   */
  init: function() {
    ProximityAuth.cryptauthController_ = new CryptAuthController();
    ProximityAuth.remoteDevicesController_ = new DeviceListController(
        document.getElementById('remote-devices-control'), true, false);
    ProximityAuth.eligibleDevicesController_ = new EligibleDevicesController();
    WebUI.getLocalState();
  }
};

/**
 * Controller for the CryptAuth controls section.
 */
class CryptAuthController {
  constructor() {
    this.elements_ = {
      localDeviceId: document.getElementById('local-device-id'),
      gcmRegistration: document.getElementById('gcm-registration'),
      currentEid: document.getElementById('current-eid'),
      enrollmentTitle: document.getElementById('enrollment-title'),
      lastEnrollment: document.getElementById('last-enrollment'),
      nextEnrollment: document.getElementById('next-enrollment'),
      enrollmentButton: document.getElementById('force-enrollment'),
      deviceSyncTitle: document.getElementById('device-sync-title'),
      lastDeviceSync: document.getElementById('last-device-sync'),
      nextDeviceSync: document.getElementById('next-device-sync'),
      deviceSyncButton: document.getElementById('force-device-sync'),
      newUserNotifButton: document.getElementById('show-new-user-notif'),
      existingUserNewHostNotifButton:
          document.getElementById('show-existing-user-new-host-notif'),
      existingUserNewChromebookNotifButton:
          document.getElementById('show-existing-user-new-chromebook-notif'),
    };

    this.elements_.enrollmentButton.onclick = this.forceEnrollment_.bind(this);
    this.elements_.deviceSyncButton.onclick = this.forceDeviceSync_.bind(this);
    this.elements_.newUserNotifButton.onclick =
        this.showNewUserNotification_.bind(this);
    this.elements_.existingUserNewHostNotifButton.onclick =
        this.showExistingUserNewHostNotification_.bind(this);
    this.elements_.existingUserNewChromebookNotifButton.onclick =
        this.showExistingUserNewChromebookNotification_.bind(this);

    this.multiDeviceSetup =
        chromeos.multideviceSetup.mojom.MultiDeviceSetup.getRemote();
  }

  /**
   * Sets the local device's ID. Note that this value is truncated since the
   * full value is very long and does not cleanly fit on the screen.
   * @param {string} deviceIdTruncated
   */
  setLocalDeviceId(deviceIdTruncated) {
    this.elements_.localDeviceId.textContent = deviceIdTruncated;
  }

  /**
   * Update the enrollment state in the UI.
   * @param {!SyncState} state
   */
  updateEnrollmentState(state) {
    this.elements_.lastEnrollment.textContent =
        this.getLastSyncTimeString_(state, 'Never enrolled');
    this.elements_.nextEnrollment.textContent =
        this.getNextRefreshString_(state);

    if (state.recoveringFromFailure) {
      this.elements_.enrollmentTitle.setAttribute('state', 'failure');
    } else if (state.operationInProgress) {
      this.elements_.enrollmentTitle.setAttribute('state', 'in-progress');
    } else {
      this.elements_.enrollmentTitle.setAttribute('state', 'synced');
    }
  }

  /**
   * Updates the device sync state in the UI.
   * @param {!SyncState} state
   */
  updateDeviceSyncState(state) {
    this.elements_.lastDeviceSync.textContent =
        this.getLastSyncTimeString_(state, 'Never synced');
    this.elements_.nextDeviceSync.textContent =
        this.getNextRefreshString_(state);

    if (state.recoveringFromFailure) {
      this.elements_.deviceSyncTitle.setAttribute('state', 'failure');
    } else if (state.operationInProgress) {
      this.elements_.deviceSyncTitle.setAttribute('state', 'in-progress');
    } else {
      this.elements_.deviceSyncTitle.setAttribute('state', 'synced');
    }
  }

  /**
   * Returns the formatted string of the time of the last sync to be displayed.
   * @param {!SyncState} syncState
   * @return {string}
   */
  getLastSyncTimeString_(syncState, neverSyncedString) {
    if (syncState.lastSuccessTime == 0)
      return neverSyncedString;
    var date = new Date(syncState.lastSuccessTime);
    return date.toLocaleDateString() + ' ' + date.toLocaleTimeString();
  }

  /**
   * Returns the formatted string of the next time to refresh to be displayed.
   * @param {!SyncState} syncState
   * @return {string}
   */
  getNextRefreshString_(syncState) {
    var deltaMillis = syncState.nextRefreshTime;
    if (deltaMillis == null)
      return 'unknown';
    if (deltaMillis == 0)
      return 'sync in progress...';

    var seconds = deltaMillis / 1000;
    if (seconds < 60)
      return Math.round(seconds) + ' seconds to refresh';

    var minutes = seconds / 60;
    if (minutes < 60)
      return Math.round(minutes) + ' minutes to refresh';

    var hours = minutes / 60;
    if (hours < 24)
      return Math.round(hours) + ' hours to refresh';

    var days = hours / 24;
    return Math.round(days) + ' days to refresh';
  }

  /**
   * Forces a CryptAuth enrollment on button click.
   */
  forceEnrollment_() {
    WebUI.forceEnrollment();
  }

  /**
   * Forces a device sync on button click.
   */
  forceDeviceSync_() {
    WebUI.forceDeviceSync();
  }

  /**
   * Shows the "new user, potential host exists" notification.
   */
  showNewUserNotification_() {
    this.showMultiDeviceSetupPromoNotification_(
        chromeos.multideviceSetup.mojom.EventTypeForDebugging.
            kNewUserPotentialHostExists);
  }

  /**
   * Shows the "existing user, new host" notification.
   */
  showExistingUserNewHostNotification_() {
    this.showMultiDeviceSetupPromoNotification_(
        chromeos.multideviceSetup.mojom.EventTypeForDebugging.
            kExistingUserConnectedHostSwitched);
  }

  /**
   * Shows the "existing user, new Chromebook" notification.
   */
  showExistingUserNewChromebookNotification_() {
    this.showMultiDeviceSetupPromoNotification_(
        chromeos.multideviceSetup.mojom.EventTypeForDebugging.
            kExistingUserNewChromebookAdded);
  }

  /**
   * Shows a "MultiDevice Setup" notification of the given type.
   * @param {!chromeos.multideviceSetup.mojom.EventTypeForDebugging} type
   */
  showMultiDeviceSetupPromoNotification_(type) {
    this.multiDeviceSetup.triggerEventForDebugging(type).then(
        function(responseParams) {
          if (responseParams.success) {
            console.log('Successfully triggered notification for type ' +
                type + '.');
          } else {
            console.error('Failed to trigger notification for type ' + type +
                '; no NotificationPresenter has been registered.');
          }
        }).catch(function(error) {
          console.error('Failed to trigger notification type. ' + error);
        });
  }
};

/**
 * Controller for a list of remote devices. These lists are displayed in a
 * number of locations on the debug page.
 */
class DeviceListController {
  constructor(rootElement, showScanButton, showToggleUnlockKeyButton) {
    this.rootElement_ = rootElement;
    this.showScanButton_ = showScanButton;
    this.showToggleUnlockKeyButton_ = showToggleUnlockKeyButton;
    this.remoteDeviceTemplate_ =
        document.getElementById('remote-device-template');
  }

  /**
   * Updates the UI with the given remote devices.
   * @param {!Array<!RemoteDevice>} remoteDevices
   */
  updateRemoteDevices(remoteDevices) {
    var existingItems =
        this.rootElement_.querySelectorAll('.remote-device');
    for (var i = 0; i < existingItems.length; ++i) {
      existingItems[i].remove();
    }

    for (var i = 0; i < remoteDevices.length; ++i) {
      this.rootElement_.appendChild(
          this.createRemoteDeviceItem_(remoteDevices[i]));
    }

    this.rootElement_.setAttribute('device-count', remoteDevices.length);
  }

  /**
   * Creates a DOM element for a given remote device.
   * @param {!RemoteDevice} remoteDevice
   * @return {!Node}
   */
  createRemoteDeviceItem_(remoteDevice) {
    var isUnlockKey = !!remoteDevice.unlockKey;
    var hasMobileHotspot = !!remoteDevice.hasMobileHotspot;
    var isArcPlusPlusEnrollment = !!remoteDevice.isArcPlusPlusEnrollment;
    var isPixelPhone = !!remoteDevice.isPixelPhone;

    var t = this.remoteDeviceTemplate_.content;
    t.querySelector('.device-connection-status').setAttribute(
        'state', remoteDevice.connectionStatus);
    t.querySelector('.device-name').textContent =
        remoteDevice.friendlyDeviceName;
    t.querySelector('.no-pii-name').textContent =
        remoteDevice.noPiiName;
    t.querySelector('.device-id').textContent =
        remoteDevice.publicKeyTruncated;
    t.querySelector('.software-features').textContent =
        remoteDevice.featureStates;
    t.querySelector('.is-unlock-key').textContent = isUnlockKey;
    t.querySelector('.supports-mobile-hotspot').textContent = hasMobileHotspot;
    t.querySelector('.is-arc-plus-plus-enrollment').textContent =
        isArcPlusPlusEnrollment;
    t.querySelector('.is-pixel-phone').textContent = isPixelPhone;
    if (!!remoteDevice.bluetoothAddress) {
      t.querySelector('.bluetooth-address-row').classList.remove('hidden');
      t.querySelector('.bluetooth-address').textContent =
          remoteDevice.bluetoothAddress;
    }

    var scanButton = t.querySelector('.device-scan');
    scanButton.classList.toggle(
        'hidden', !this.showScanButton_ || !isUnlockKey);
    scanButton.textContent =
        remoteDevice.connectionStatus == 'disconnected'
            ? 'EasyUnlock Scan' : 'EasyUnlock Disconnect';
    t.querySelector('.device-toggle-key').classList.toggle(
        'hidden', !this.showToggleUnlockKeyButton_ || !isUnlockKey);

    var element = document.importNode(this.remoteDeviceTemplate_.content, true);

    // Initialize buttons on new element.
    element.querySelector('.device-scan').onclick =
        this.scanForDevice_.bind(this, remoteDevice.publicKey);
    element.querySelector('.device-toggle-key').onclick =
        this.toggleUnlockKey_.bind(
            this, remoteDevice.publicKey, !remoteDevice.unlockKey);

    return element;
  }

  /**
   * Button handler to start scanning and connecting to a device.
   * @param {string} publicKey
   */
  scanForDevice_(publicKey) {
    WebUI.toggleConnection(publicKey);
  }

  /**
   * Button handler to toggle a device as an unlock key.
   * @param {string} publicKey
   * @param {boolean} makeUnlockKey
   */
  toggleUnlockKey_(publicKey, makeUnlockKey) {
    console.log(publicKey);
    WebUI.toggleUnlockKey(publicKey, makeUnlockKey);
  }
}

/**
 * Controller for the 'Eligible Unlock Keys' controls.
 */
class EligibleDevicesController {
  constructor() {
    this.eligibleDeviceList_ = new DeviceListController(
        document.getElementById('eligible-devices-list'), false, true);
    this.ineligibleDeviceList_ = new DeviceListController(
        document.getElementById('ineligible-devices-list'), false, false);
    this.button_ = document.getElementById('find-eligible-devices');
    this.button_.onclick = this.findEligibleUnlockDevices_.bind(this);
  }

  /**
   * Updates the UI with the fetched eligible and ineligible devices.
   * @param {!Array<!RemoteDevice>} eligibleDevices
   * @param {!Array<!RemoteDevice>} ineligibleDevices
   */
  updateEligibleDevices(eligibleDevices, ineligibleDevices) {
    this.eligibleDeviceList_.updateRemoteDevices(eligibleDevices);
    this.ineligibleDeviceList_.updateRemoteDevices(ineligibleDevices);
  }

  /**
   * Button handler to fetch eligible unlock devices.
   */
  findEligibleUnlockDevices_() {
    WebUI.findEligibleUnlockDevices();
  }
}

/**
 * Interface for the native WebUI to call into our JS.
 */
const LocalStateInterface = {
  /**
   * @param {string} localDeviceId
   * @param {!SyncState} enrollmentState
   * @param {!SyncState} deviceSyncState
   * @param {!Array<!RemoteDevice>} remoteDevices
   */
  onGotLocalState: function(
      localDeviceId, enrollmentState, deviceSyncState, remoteDevices) {
    LocalStateInterface.setLocalDeviceId(localDeviceId);
    LocalStateInterface.onEnrollmentStateChanged(enrollmentState);
    LocalStateInterface.onDeviceSyncStateChanged(deviceSyncState);
    LocalStateInterface.onRemoteDevicesChanged(remoteDevices);
  },

  /** @param {string} localDeviceId */
  setLocalDeviceId: function(localDeviceId) {
    ProximityAuth.cryptauthController_.setLocalDeviceId(localDeviceId);
  },

  /** @param {!SyncState} enrollmentState */
  onEnrollmentStateChanged: function(enrollmentState) {
    ProximityAuth.cryptauthController_.updateEnrollmentState(enrollmentState);
  },

  /** @param {!SyncState} deviceSyncState */
  onDeviceSyncStateChanged: function(deviceSyncState) {
    ProximityAuth.cryptauthController_.updateDeviceSyncState(deviceSyncState);
  },

  /** @param {!Array<!RemoteDevice>} remoteDevices */
  onRemoteDevicesChanged: function(remoteDevices) {
    ProximityAuth.remoteDevicesController_.updateRemoteDevices(remoteDevices);
  }
};

/**
 * Interface for the native WebUI to call into our JS.
 */
const CryptAuthInterface = {
  /**
   * @param {!Array<!RemoteDevice>} eligibleDevices
   * @param {!Array<!RemoteDevice>} ineligibleDevices
   */
  onGotEligibleDevices: function(eligibleDevices, ineligibleDevices) {
    ProximityAuth.eligibleDevicesController_.updateEligibleDevices(
        eligibleDevices, ineligibleDevices);
  }
};

document.addEventListener('DOMContentLoaded', function() {
  WebUI.onWebContentsInitialized();
  Logs.init();
  ProximityAuth.init();
});
