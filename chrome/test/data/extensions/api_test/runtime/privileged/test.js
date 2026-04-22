// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const fail = chrome.test.fail;
const succeed = chrome.test.succeed;

const isServiceWorker = ('ServiceWorkerGlobalScope' in self);

const EXTENSION_ID = 'iegclhlplifhodhkoafiokenjoapiobj';
const SERVICE_WORKER_SCRIPT_NAME = 'test.js';

function checkIsDefined(prop) {
  if (!chrome.runtime) {
    fail('chrome.runtime is not defined');
    return false;
  }
  if (!chrome.runtime[prop]) {
    fail(`chrome.runtime.${prop} is not undefined`);
    return false;
  }
  return true;
}

function getLocation() {
  return isServiceWorker ? self.location.toString() : window.location.href;
}

function getPath() {
  return isServiceWorker ? `/${SERVICE_WORKER_SCRIPT_NAME}` :
                           '/_generated_background_page.html';
}

chrome.test.runTests([

  function testID() {
    if (!checkIsDefined('id')) {
      return;
    }
    assertEq(EXTENSION_ID, chrome.runtime.id);
    succeed();
  },

  function testGetURL() {
    if (!checkIsDefined('getURL')) {
      return;
    }
    assertEq(
        `chrome-extension://${chrome.runtime.id}${getPath()}`, getLocation());
    succeed();
  },

  function testGetManifest() {
    if (!checkIsDefined('getManifest')) {
      return;
    }
    const manifest = chrome.runtime.getManifest();
    if (!manifest || !manifest.background ||
        !(manifest.background.scripts || manifest.background.service_worker)) {
      fail('Extension has no background or worker script.');
      return;
    }
    assertEq('chrome.runtime API Test', manifest.name);
    assertEq('1', manifest.version);
    if (isServiceWorker) {
      assertEq(3, manifest.manifest_version);
    } else {
      assertEq(2, manifest.manifest_version);
    }
    if (manifest.background.scripts) {
      assertEq(['test.js'], manifest.background.scripts);
    } else {
      assertEq(SERVICE_WORKER_SCRIPT_NAME, manifest.background.service_worker);
    }
    succeed();
  },

  function testGetVersion() {
    if (!checkIsDefined('getVersion')) {
      return;
    }
    const version = chrome.runtime.getVersion();
    assertEq('1', version);
    succeed();
  },

]);
