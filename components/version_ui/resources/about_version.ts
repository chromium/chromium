// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The handle* functions below are called internally on promise
// resolution, unlike the other return* functions, which are called
// asynchronously by the host.

// clang-format off
// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import '/strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
// <if expr="is_chromeos or is_win">
import {addWebUiListener} from 'chrome://resources/js/cr.js';
// </if>
// <if expr="chromeos_ash">
import {$} from 'chrome://resources/js/util.js';
// </if>
// clang-format on


/**
 * Truncate string if it's too long to show.
 * @param str The original string.
 * @returns If |str| length is less or equal than 60, return |str|.
 * Otherwise return the truncated |str|, appended with '...' in the end.
 */
function truncateString(str: string): string {
  // 60 is a magic number which show nicely on the page.
  const maxLength = 60;
  if (str.length <= maxLength) {
    return str;
  }
  return str.substring(0, maxLength) + '...';
}

/**
 * Promise resolution handler for variations list and command line equivalent.
 */
function handleVariationInfo(
    {variationsList, variationsCmd}:
        {variationsList: string[], variationsCmd: string}) {
  getRequiredElement('variations-section').hidden = !variationsList.length;
  for (const item of variationsList) {
    getRequiredElement('variations-list')
        .appendChild(document.createTextNode(item));
    getRequiredElement('variations-list')
        .appendChild(document.createElement('br'));
  }

  const includeVariationsCmd = location.search.includes('show-variations-cmd');
  if (variationsCmd !== '') {
    getRequiredElement('variations-cmd-section').hidden = false;
    getRequiredElement('copy-variations-to-clipboard').hidden =
        includeVariationsCmd;
    if (includeVariationsCmd) {
      getRequiredElement('variations-cmd').textContent = variationsCmd;
    } else {
      getRequiredElement('variations-cmd').textContent =
          truncateString(variationsCmd);
      getRequiredElement('variations-cmd').dataset['value'] = variationsCmd;
    }
  }
}

/**
 * Promise resolution handler for the executable and profile paths to display.
 * @param execPath The executable path to display.
 * @param profilePath The profile path to display.
 */
function handlePathInfo(
    {execPath, profilePath}: {execPath: string, profilePath: string}) {
  getRequiredElement('executable_path').textContent = execPath;
  getRequiredElement('profile_path').textContent = profilePath;
}

// <if expr="chromeos_lacros or is_win">
/**
 * Callback from the backend with the OS version to display.
 * @param osVersion The OS version to display.
 */
function returnOsVersion(osVersion: string) {
  getRequiredElement('os_version').textContent = osVersion;
}
// </if>

// <if expr="is_chromeos">
/**
 * Callback from the backend with the ChromeOS platform version to display.
 * @param platformVersion The platform version to display.
 */
function returnPlatformVersion(platformVersion: string) {
  getRequiredElement('platform_version').textContent = platformVersion;
}

/**
 * Callback from the backend with the firmware version to display.
 */
function returnFirmwareVersion(firmwareVersion: string) {
  getRequiredElement('firmware_version').textContent = firmwareVersion;
}

/**
 * Callback from the backend with the ARC Android SDK version to display.
 * @param arcAndroidSdkVersion The ARC Android SDK version to display,
 *     already localized.
 */
function returnArcAndArcAndroidSdkVersions(arcAndroidSdkVersion: string) {
  getRequiredElement('arc_holder').hidden = false;
  getRequiredElement('arc_and_arc_android_sdk_versions').textContent =
      arcAndroidSdkVersion;
}

/**
 * Callback from chromeosInfoPrivate with the value of the customization ID.
 */
function returnCustomizationId(response: {[customizationId: string]: any}) {
  if (!response['customizationId']) {
    return;
  }
  getRequiredElement('customization_id_holder').hidden = false;
  getRequiredElement('customization_id').textContent =
      response['customizationId'];
}

// </if>

// <if expr="chromeos_ash">
/**
 * Callback from the backend to inform if Lacros is enabled or not.
 * @param enabled True if it is enabled.
 */
function returnLacrosEnabled(enabled: string) {
  getRequiredElement('os-link-container').hidden = !enabled;

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

async function copyVersionToClipboard() {
  await navigator.clipboard.writeText(
      getRequiredElement('copy-content').innerText);
  announceCopy('copy_notice');
}

async function copyVariationsToClipboard() {
  const cmdLine =
      getRequiredElement('variations-cmd').dataset['value'] as string;
  await navigator.clipboard.writeText(cmdLine);
  announceCopy('copy_variations_notice');
}

/**
 * Announce the copy action when screen reader is on.
 * @param id The id string for the notice.
 */
function announceCopy(id: string) {
  const messagesDiv = getRequiredElement('messages');
  messagesDiv.innerHTML = window.trustedTypes!.emptyHTML;

  // <if expr="is_macosx">
  // VoiceOver on Mac does not seem to consistently read out the contents of
  // a static alert element. Toggling the role of alert seems to force VO
  // to consistently read out the messages.
  messagesDiv.removeAttribute('role');
  messagesDiv.setAttribute('role', 'alert');
  // </if>

  const div = document.createElement('div');
  div.innerText = loadTimeData.getString(id);
  messagesDiv.append(div);
}

// <if expr="chromeos_lacros">
function copyOSContentToClipboard() {
  navigator.clipboard.writeText(
      getRequiredElement('copy-os-content').innerText);
}
// </if>

/* All the work we do onload. */
function initialize() {
  // <if expr="chromeos_lacros or is_win">
  addWebUiListener('return-os-version', returnOsVersion);
  // </if>

  // <if expr="is_chromeos">
  addWebUiListener('return-platform-version', returnPlatformVersion);
  addWebUiListener('return-firmware-version', returnFirmwareVersion);
  addWebUiListener(
      'return-arc-and-arc-android-sdk-versions',
      returnArcAndArcAndroidSdkVersions);
  getRequiredElement('arc_holder').hidden = true;
  chrome.chromeosInfoPrivate.get(['customizationId'])
      .then(returnCustomizationId);
  // </if>

  // <if expr="chromeos_ash">
  addWebUiListener('return-lacros-enabled', returnLacrosEnabled);
  // </if>
  // <if expr="chromeos_lacros">
  // We always display the container in Lacros
  getRequiredElement('os-link-container').hidden = false;
  // </if>

  chrome.send('requestVersionInfo');
  const includeVariationsCmd = location.search.includes('show-variations-cmd');
  sendWithPromise('requestVariationInfo', includeVariationsCmd)
      .then(handleVariationInfo);
  sendWithPromise('requestPathInfo').then(handlePathInfo);

  if (getRequiredElement('variations-seed').textContent !== '') {
    getRequiredElement('variations-seed-section').hidden = false;
  }

  if (getRequiredElement('sanitizer').textContent !== '') {
    getRequiredElement('sanitizer-section').hidden = false;
  }

  getRequiredElement('copy-to-clipboard')
      .addEventListener('click', copyVersionToClipboard);

  getRequiredElement('copy-variations-to-clipboard')
      .addEventListener('click', copyVariationsToClipboard);

  // <if expr="chromeos_lacros">
  getRequiredElement('copy-os-content-to-clipboard')
      .addEventListener('click', copyOSContentToClipboard);
  // </if>
}

document.addEventListener('DOMContentLoaded', initialize);
