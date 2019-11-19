// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The handle* functions below are called internally on promise
// resolution, unlike the other return* functions, which are called
// asynchronously by the host.

/**
 * Promise resolution handler for variations list and command line equivalent.
 * @param {{variationsList: !Array<string>, variationsCmd: string=}}
 */
function handleVariationInfo({variationsList, variationsCmd}) {
  $('variations-section').hidden = !variationsList.length;
  $('variations-list').appendChild(
      parseHtmlSubset(variationsList.join('<br>'), ['BR']));

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

/**
 * Promise resolution handler for the Flash version to display.
 * @param {string} flashVersion The Flash version to display.
 */
function handlePluginInfo(flashVersion) {
  $('flash_version').textContent = flashVersion;
}

/**
 * Callback from the backend with the OS version to display.
 * @param {string} osVersion The OS version to display.
 */
function returnOsVersion(osVersion) {
  $('os_version').textContent = osVersion;
}

/**
 * Callback from the backend with the firmware version to display.
 * @param {string} firmwareVersion
 */
function returnOsFirmwareVersion(firmwareVersion) {
  $('firmware_version').textContent = firmwareVersion;
}

/**
 * Callback from the backend with the ARC version to display.
 * @param {string} arcVersion The ARC version to display.
 */
function returnARCVersion(arcVersion) {
  $('arc_version').textContent = arcVersion;
  $('arc_holder').hidden = !arcVersion;
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

/* All the work we do onload. */
function onLoadWork() {
  chrome.send('requestVersionInfo');
  const includeVariationsCmd = location.search.includes("show-variations-cmd");
  cr.sendWithPromise('requestVariationInfo', includeVariationsCmd)
      .then(handleVariationInfo);
  cr.sendWithPromise('requestPluginInfo').then(handlePluginInfo);
  cr.sendWithPromise('requestPathInfo').then(handlePathInfo);

  if (cr.isChromeOS) {
    $('arc_holder').hidden = true;
    chrome.chromeosInfoPrivate.get(['customizationId'], returnCustomizationId);
  }
  if ($('sanitizer').textContent != '') {
    $('sanitizer-section').hidden = false;
  }
}

document.addEventListener('DOMContentLoaded', onLoadWork);
