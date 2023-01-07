// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  if (isManaged()) {
    document.body.querySelector('#managed-info').classList.remove('hidden');
  } else {
    document.body.querySelector('#unmanaged-info').classList.remove('hidden');
  }
});
