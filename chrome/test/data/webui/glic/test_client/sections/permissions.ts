// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file implements listeners/actions for multiple permission related
// sections in the test client.

import type {PermissionSwitchName} from '../client.js';
import {getBrowser, logMessage, permissionSwitches} from '../client.js';
import {$} from '../page_element_types.js';

async function checkMicrophonePermission():
    Promise<'success'|'localDenial'|'osDenial'|'unknown'> {
  try {
    await navigator.mediaDevices.getUserMedia({audio: true});
    return 'success';
  } catch (error) {
    if (error instanceof DOMException && error.name === 'NotAllowedError') {
      // Use the GlicBrowserHost to check the permission state.
      const micPermissionStatus = permissionSwitches['microphone'].checked;
      if (!micPermissionStatus) {
        return 'localDenial';
      } else {
        return 'osDenial';
      }
    } else {
      console.error(error);
    }
    return 'unknown';
  }
}

// Chrome Permissions.
$.testPermissionSwitch.addEventListener('click', () => {
  const selectedPermission = $.permissionSelect.value as PermissionSwitchName;
  const isEnabled = $.enabledSelect.value === 'true';
  if (!permissionSwitches[selectedPermission]) {
    console.error('Unknown permission: ' + selectedPermission);
    return;
  }
  if (selectedPermission === 'microphone') {
    getBrowser()!.setMicrophonePermissionState!(isEnabled);
  } else if (selectedPermission === 'geolocation') {
    getBrowser()!.setLocationPermissionState!(isEnabled);
  } else if (selectedPermission === 'tabContext') {
    getBrowser()!.setTabContextPermissionState!(isEnabled);
  }
  logMessage(
      `Setting permission ${selectedPermission} to ${isEnabled}.`,
  );
});

// MacOS Permissions.
$.openLocalSettingsButton.addEventListener('click', () => {
  getBrowser()!.openGlicSettingsPage!();
});
$.openOsSettingsButton.addEventListener('click', () => {
  getBrowser()!.openOsPermissionSettingsMenu!('media');
});
$.openOsLocationSettingsButton.addEventListener('click', () => {
  getBrowser()!.openOsPermissionSettingsMenu!('geolocation');
});
$.openOsLocationSettings.addEventListener('click', () => {
  getBrowser()!.openOsPermissionSettingsMenu!('geolocation');
});
$.openOsMicrophoneSettings.addEventListener('click', () => {
  getBrowser()!.openOsPermissionSettingsMenu!('media');
});
$.openGlicLocationSettingsButton.addEventListener('click', () => {
  getBrowser()!.openGlicSettingsPage!();
});
$.getOsMicrophonePermissionButton.addEventListener('click', async () => {
  const permission = await getBrowser()!.getOsMicrophonePermissionStatus!();
  $.osMicrophonePermissionResult.textContent =
      `OS Microphone Permission: ${permission}`;
});

// Mic Start Permissions Flow Demo.
$.startMic.addEventListener('click', async () => {
  const permissionResult = await checkMicrophonePermission();

  $.startMic.style.display = 'none';

  if (permissionResult === 'success') {
    $.successUI.style.display = 'block';
  } else if (permissionResult === 'localDenial') {
    $.localDenialUI.style.display = 'block';
  } else if (permissionResult === 'osDenial') {
    $.osDenialUI.style.display = 'block';
  }
});

// Test Location Access.
$.getlocation.addEventListener('click', async () => {
  if (navigator.geolocation) {
    try {
      $.locationStatus.innerText = 'Requesting geolocation...';
      const position =
          await new Promise<GeolocationPosition>((resolve, reject) => {
            navigator.geolocation.getCurrentPosition(resolve, reject);
          });
      const latitude = position.coords.latitude;
      const longitude = position.coords.longitude;
      const accuracy = position.coords.accuracy;

      $.location.innerHTML = `
          Latitude: ${latitude}<br>
          Longitude: ${longitude}<br>
          Accuracy: ${accuracy} meters
        `;
      $.locationStatus.innerText = `Location Received.`;
    } catch (error) {
      $.locationStatus.innerText = `Error: ${error}`;
      $.location.innerHTML = ``;
      if (error instanceof GeolocationPositionError) {
        if (error.code === 1) {
          $.locationStatus.innerText = `Permission Denied.`;
          if (!permissionSwitches['osGeolocation'].checked) {
            $.locationOsErrorUI.style.display = 'block';
          } else if (!permissionSwitches['geolocation'].checked) {
            $.locationGlicErrorUI.style.display = 'block';
          }
        }
      }
    }
  } else {
    $.location.innerHTML = 'Geolocation is not supported by this browser.';
  }
});
