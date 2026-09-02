// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function reloadButtonClicked() {
  // Skeleton handler for reload button.
}

function signinButtonClicked() {
  // Skeleton handler for sign-in button.
}

document.addEventListener('DOMContentLoaded', () => {
  const reloadButton = document.getElementById('reload-button');
  if (reloadButton) {
    reloadButton.addEventListener('click', reloadButtonClicked);
  }
  const signinButton = document.getElementById('signin-button');
  if (signinButton) {
    signinButton.addEventListener('click', signinButtonClicked);
  }
});
