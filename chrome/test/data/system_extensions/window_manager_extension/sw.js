// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

chromeos.windowManagement.addEventListener('start', e => {
  console.log('start event fired');
});

chromeos.windowManagement.addEventListener('acceleratordown', e => {
  const event = {
    type: "acceleratordown",
    name: e.acceleratorName,
    repeat: e.repeat
  };
  console.log(JSON.stringify(event));
});

chromeos.windowManagement.addEventListener('acceleratorup', e => {
  const event = {
    type: "acceleratorup",
    name: e.acceleratorName,
    repeat: e.repeat
  };
  console.log(JSON.stringify(event));
});

chromeos.windowManagement.addEventListener('windowclosed', e => {
  console.log(JSON.stringify(e.window.id));
});
