// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic example of establishing comms with the backend.

// There must be only one listener returning values.
theBrowserProxy.callbackRouter.getBar.addListener((foo) => {
  console.log('GetBar(' + foo + ')');
  return Promise.resolve({bar: 'baz'});
});

// Listen-only callbacks can be multiple.
theBrowserProxy.callbackRouter.onSomethingHappened.addListener(
    (something, other) => {
      console.log('OnSomethingHappened(' + something + ', ' + other + ')');
    });
theBrowserProxy.callbackRouter.onSomethingHappened.addListener(
    (something, other) => {
      console.log('eh? ' + something + '. what? ' + other);
    });

document.addEventListener('DOMContentLoaded', () => {
  console.info('File manager launched ...');
});
