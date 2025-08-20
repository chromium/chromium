// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  let getActiveElement = function() {
    let activeElement = document.activeElement;
    while (activeElement && activeElement.shadowRoot &&
           activeElement.shadowRoot.activeElement) {
      activeElement = activeElement.shadowRoot.activeElement;
    }
    return activeElement;
  };
  let backPressEvent = new KeyboardEvent(
      'keydown',
      {bubbles: true, key: 'BrowserBack', cancelable: true, composed: true});
  let activeElement = getActiveElement();
  if (activeElement) {
    activeElement.dispatchEvent(backPressEvent);
  } else {
    document.dispatchEvent(backPressEvent);
  }
  backPressEvent.defaultPrevented;
};
