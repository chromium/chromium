// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.js';

const HIDDEN_CLASS: string = 'hidden';

function setupEvents() {
  $('details-certerror-button').addEventListener('click', function(_event) {
    toggleHidden('details-certerror', 'details-certerror-button');
  });
  $('details-connectnetwork-button')
      .addEventListener('click', function(_event) {
        toggleHidden('details-connectnetwork', 'details-connectnetwork-button');
      });
  $('details-clock-button').addEventListener('click', function(_event) {
    toggleHidden('details-clock', 'details-clock-button');
  });
  if (loadTimeData.getBoolean('isWindows')) {
    $('windows-only').classList.remove(HIDDEN_CLASS);
    $('details-mitmsoftware-button')
        .addEventListener('click', function(_event) {
          toggleHidden('details-mitmsoftware', 'details-mitmsoftware-button');
        });
  }
  switch (window.location.hash) {
    case '#' + loadTimeData.getInteger('certCommonNameInvalid'):
    case '#' + loadTimeData.getInteger('certAuthorityInvalid'):
    case '#' + loadTimeData.getInteger('certWeakSignatureAlgorithm'):
    case '#' + loadTimeData.getInteger('certKnownInterceptionBlocked'):
      toggleHidden('details-certerror', 'details-certerror-button');
      break;
    case '#' + loadTimeData.getInteger('certExpired'):
      toggleHidden('details-clock', 'details-clock-button');
      break;
  }
}

function toggleHidden(className: string, buttonName: string) {
  const hiddenDetails = $(className).classList.toggle(HIDDEN_CLASS);
  $(buttonName).innerText = loadTimeData.getString(
      hiddenDetails ? 'connectionHelpShowMore' : 'connectionHelpShowLess');
}

document.addEventListener('DOMContentLoaded', setupEvents);
