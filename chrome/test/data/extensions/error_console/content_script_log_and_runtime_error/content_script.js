// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function logMessage() {
  console.log('logged message');
}

function warnMessage() {
  console.warn('warned message');
}

logMessage();
warnMessage();

var bar = undefined;
bar.foo = 'baz';
