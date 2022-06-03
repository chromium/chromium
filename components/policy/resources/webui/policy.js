// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Page} from './policy_base.js';

// Have the main initialization function be called when the page finishes
// loading.
const page = Page.getInstance();
document.addEventListener('DOMContentLoaded', () => {
  page.initialize();
});
