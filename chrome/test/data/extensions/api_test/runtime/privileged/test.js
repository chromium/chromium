// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;

const isServiceWorker = ('ServiceWorkerGlobalScope' in self);
const extensionId = 'iegclhlplifhodhkoafiokenjoapiobj';
const serviceWorkerScriptName = 'generated_service_worker__.js';

function checkIsDefined(prop) {
  if (!chrome.runtime) {
    fail('chrome.runtime is not defined');
    return false;
  }
  if (!chrome.runtime[prop]) {
    fail('chrome.runtime.' + prop + ' is not undefined');
    return false;
  }
  return true;
}

function getLocation() {
  return isServiceWorker ? self.location.toString() : window.location.href;
};

function getPath() {
  return isServiceWorker ? '/' + serviceWorkerScriptName
      : '/_generated_background_page.html';
};

chrome.test.runTests([

  function testID() {
    if (!checkIsDefined('id'))
      return;
    assertEq(extensionId, chrome.runtime.id);
    succeed();
  },

  function testGetURL() {
    if (!checkIsDefined('getURL'))
      return;
    assertEq('chrome-extension://' + chrome.runtime.id + getPath(),
             getLocation());
    succeed();
  },

  function testGetManifest() {
    if (!checkIsDefined('getManifest'))
      return;
    var manifest = chrome.runtime.getManifest();
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
      assertEq(serviceWorkerScriptName, manifest.background.service_worker);
    }
    succeed();
  },

]);
