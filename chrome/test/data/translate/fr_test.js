// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof world != 'undefined') {
  console.log('CSP does not work correctly.');
  console.log(world);
  document.title = 'FAIL';
} else {
  world='main';
}
