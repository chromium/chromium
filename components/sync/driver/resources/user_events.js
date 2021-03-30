// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {writeUserEvent} from './chrome_sync.js';

function write() {
  writeUserEvent(
      $('event-time-usec-input').value, $('navigation-id-input').value);
}

document.addEventListener('DOMContentLoaded', () => {
  $('create-event-button').addEventListener('click', write);
}, false);
