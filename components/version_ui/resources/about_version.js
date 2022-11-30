// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The handle* functions below are called internally on promise
// resolution, unlike the other return* functions, which are called
// asynchronously by the host.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './strings.m.js';
import {addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.js';

/**
 * Promise resolution handler for variations list and command line equivalent.
 * @param {{variationsList: !Array<string>, variationsCmd: string=}}
 */
function handleVariationInfo({variationsList, variationsCmd}) {
  $('variations-section').hidden = !variationsList.length;
  for (const item of variationsList) {
    $('variations-list').appendChild(document.createTextNode(item));
    $('variations-list').appendChild(document.createElement('br'));
  }

  if (variationsCmd) {
    $('variations-cmd-section').hidden = !variationsCmd;
    $('variations-cmd').textContent = variationsCmd;
  }
}

/**
 * Promise resolution handler for the executable and profile paths to display.
 * @param {string} execPath The executable path to display.
 * @param {string} profilePath The profile path to display.
 */
function handlePathInfo({execPath, profilePath}) {
  $('executable_path').textContent = execPath;
  $('profile_path').textContent = profilePath;
}

// <if expr="chromeos_ash or is_win">
/**
 * Callback from the backend with the OS version to display.
 * @param {string} osVersion The OS version to display.
 */
function returnOsVersion(osVersion) {
  $('os_version').textContent = osVersion;
}
// </if>

// <if expr="chromeos_ash">
/**
 * Callback from the backend with the firmware version to display.
 * @param {string} firmwareVersion
 */
function returnOsFirmwareVersion(firmwareVersion) {
  $('firmware_version').textContent = firmwareVersion;
}

/**
 * Callback from the backend with the ARC Android SDK version to display.
 * @param {string} arcAndroidSdkVersion The ARC Android SDK version to display,
 *     already localized.
 */
function returnArcAndArcAndroidSdkVersions(arcAndroidSdkVersion) {
  $('arc_holder').hidden = false;
  $('arc_and_arc_android_sdk_versions').textContent = arcAndroidSdkVersion;
}

/**
 * Callback from chromeosInfoPrivate with the value of the customization ID.
 * @param {!{customizationId: string}} response
 */
function returnCustomizationId(response) {
  if (!response.customizationId) {
    return;
  }
  $('customization_id_holder').hidden = false;
  $('customization_id').textContent = response.customizationId;
}
// </if>

// <if expr="is_chromeos">
/**
 * Callback from the backend to inform if Lacros is primary or not.
 * @param {string} isPrimary True if it is primary.
 */
function returnLacrosPrimary(isPrimary) {
  $('os-link-container').hidden = !isPrimary;

  const crosUrlRedirectButton = $('os-link-href');
  if (crosUrlRedirectButton) {
    crosUrlRedirectButton.onclick = crosUrlVersionRedirect;
  }
}

/**
 * Called when the user clicks on the os-link-href button.
 */
function crosUrlVersionRedirect() {
  chrome.send('crosUrlVersionRedirect');
}

// </if>

function copyToClipboard() {
  navigator.clipboard.writeText($('copy-content').innerText);
}

// <if expr="chromeos_lacros">
function copyOSContentToClipboard() {
  navigator.clipboard.writeText($('copy-os-content').innerText);
}
// </if>

/* All the work we do onload. */
function onLoadWork() {
  // <if expr="chromeos_ash or is_win">
  addWebUIListener('return-os-version', returnOsVersion);
  // </if>
  // <if expr="chromeos_ash">
  addWebUIListener('return-os-firmware-version', returnOsFirmwareVersion);
  addWebUIListener(
      'return-arc-and-arc-android-sdk-versions',
      returnArcAndArcAndroidSdkVersions);
  // </if>
  // <if expr="is_chromeos">
  addWebUIListener('return-lacros-primary', returnLacrosPrimary);
  // </if>

  chrome.send('requestVersionInfo');
  const includeVariationsCmd = location.search.includes('show-variations-cmd');
  sendWithPromise('requestVariationInfo', includeVariationsCmd)
      .then(handleVariationInfo);
  sendWithPromise('requestPathInfo').then(handlePathInfo);

  // <if expr="chromeos_ash">
  $('arc_holder').hidden = true;
  chrome.chromeosInfoPrivate.get(['customizationId'], returnCustomizationId);
  // </if>

  if ($('sanitizer').textContent !== '') {
    $('sanitizer-section').hidden = false;
  }

  $('copy-to-clipboard').addEventListener('click', copyToClipboard);

  // <if expr="chromeos_lacros">
  $('copy-os-content-to-clipboard')
      .addEventListener('click', copyOSContentToClipboard);
  // </if>
}

document.addEventListener('DOMContentLoaded', onLoadWork);
