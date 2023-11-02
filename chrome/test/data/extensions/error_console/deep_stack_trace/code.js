// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function explodes() {
  console.error('Boom!');
}

function inTheMiddle() {
  explodes();
}

function calledFirst() {
  inTheMiddle();
}

calledFirst();
