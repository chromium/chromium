// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {writeUserEvent} from './chrome_sync.js';

function write() {
  const timeInput =
      document.querySelector<HTMLInputElement>('#event-time-usec-input');
  const navigationInput =
      document.querySelector<HTMLInputElement>('#navigation-id-input');
  assert(timeInput && navigationInput);
  writeUserEvent(timeInput.value, navigationInput.value);
}

document.addEventListener('DOMContentLoaded', () => {
  const button = document.querySelector<HTMLElement>('#create-event-button');
  assert(button);
  button.addEventListener('click', write);
}, false);
