// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function fromPx(pxValue) {
  return parseInt(pxValue.replace('px', ''), 10);
}

function getSafeAreas() {
  const e = document.getElementById('target');
  const style = window.getComputedStyle(e, null);
  return {
    top: fromPx(style.getPropertyValue('margin-top')),
    left: fromPx(style.getPropertyValue('margin-left')),
    right: fromPx(style.getPropertyValue('margin-right')),
    bottom: fromPx(style.getPropertyValue('margin-bottom'))
  };
}
