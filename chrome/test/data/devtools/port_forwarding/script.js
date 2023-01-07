// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onLoad() {
  document.body.textContent = 'content';
}

function getBodyTextContent() {
  return document.body.textContent;
}

function getBodyMarginLeft() {
  return window.getComputedStyle(document.body).marginLeft;
}
