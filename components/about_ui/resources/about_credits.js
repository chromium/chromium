// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-restricted-properties */
function $(id) {
  return document.getElementById(id);
}
/* eslint-enable no-restricted-properties */

document.addEventListener('DOMContentLoaded', function() {
  // <if expr="chromeos">
  const keyboardUtils = document.createElement('script');

  const staticUrlPolicy = trustedTypes.createPolicy(
      'credits-static',
      {createScriptURL: () => 'chrome://credits/keyboard_utils.js'});

  // TODO(Jun.Kokatsu@microsoft.com): remove an empty string argument
  // once supported.
  // https://github.com/w3c/webappsec-trusted-types/issues/278
  keyboardUtils.src = staticUrlPolicy.createScriptURL('');
  document.body.appendChild(keyboardUtils);
  // </if>

  $('print-link').hidden = false;
  $('print-link').onclick = function() {
    window.print();
    return false;
  };

  document.addEventListener('keypress', function(e) {
    // Make the license show/hide toggle when the Enter is pressed.
    if (e.keyCode === 0x0d && e.target.tagName === 'LABEL') {
      e.target.previousElementSibling.click();
    }
  });
});
