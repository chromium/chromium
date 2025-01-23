// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

document.addEventListener('DOMContentLoaded', () => {
  if (loadTimeData.getString('isManaged') === 'true') {
    getRequiredElement('managed-info').classList.remove('hidden');
  } else {
    getRequiredElement('unmanaged-info').classList.remove('hidden');
  }
});
