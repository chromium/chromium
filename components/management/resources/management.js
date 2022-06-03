// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  if (isManaged()) {
    $('managed-info').classList.remove('hidden');
  } else {
    $('unmanaged-info').classList.remove('hidden');
  }
});
