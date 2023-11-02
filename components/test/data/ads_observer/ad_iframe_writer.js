// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates and iframe and appends it to the body element. Make sure the caller
// has a body element!
function createAdIframe() {
  const frame = document.createElement('iframe');
  document.body.appendChild(frame);
  return frame;
}

function createAdIframeWithSrc(src) {
  const frame = document.createElement('iframe');
  frame.src = src;
  document.body.appendChild(frame);
  return frame;
}

function createLazyAdIframeWithSrc(src) {
  const frame = document.createElement('iframe');
  frame.src = src;
  frame.loading = 'lazy';
  document.body.appendChild(frame);
  return frame;
}

function createAdIframeAtRect(x, y, width, height) {
  const frame = document.createElement('iframe');
  frame.style.border = '0px none transparent';
  frame.style.overflow = 'hidden';
  frame.style.position = 'fixed';
  frame.style.left = x;
  frame.style.top = y;
  frame.scrolling = 'no';
  frame.frameborder = '0';
  frame.allowTransparency = 'true';
  frame.width = width;
  frame.height = height;
  document.body.appendChild(frame);
  return frame;
}

function createStickyAdIframeAtBottomOfViewport(width, height) {
  const frame = document.createElement('iframe');
  frame.style.position = 'fixed';
  frame.style.bottom = '0px';
  frame.scrolling = 'no';
  frame.width = width;
  frame.height = height;
  document.body.appendChild(frame);
  return frame;
}

document.scriptExecuted = true;
