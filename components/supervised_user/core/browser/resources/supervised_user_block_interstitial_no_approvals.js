// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Return the element with the given id. */
function $(id) {
  return document.body.querySelector(`#${id}`);
}

/** Perform all initialization that can be done at DOMContentLoaded time. */
function initialize() {
  $('back-button').onclick = function(event) {
    supervisedUserErrorPageController.goBack();
  };
  $('learn-more-button').onclick = function(event) {
    supervisedUserErrorPageController.learnMore();
  };
  // Focus the top-level div for screen readers.
  $('frame-blocked').focus();
}

document.addEventListener('DOMContentLoaded', initialize);
