// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the declarativeContent API.

var setIcon = require('setIcon').setIcon;

apiBridge.registerCustomHook(function(api) {
  var declarativeContent = api.compiledApi;

  // Validation for most types is done in the native C++ with native bindings,
  // but setIcon is funny (and sadly broken). Ideally, we can move this
  // validation entirely into the native code, and this whole file can go
  // away.
  var nativeSetIcon = declarativeContent.SetIcon;
  declarativeContent.SetIcon = function(parameters) {
    // TODO(devlin): This is very, very wrong. setIcon() is potentially
    // asynchronous (in the case of a path being specified), which means this
    // becomes an "asynchronous constructor". Errors can be thrown *after* the
    // `new declarativeContent.SetIcon(...)` call, and in the async cases,
    // this wouldn't work when we immediately add the action via an API call
    // (e.g.,
    //   chrome.declarativeContent.onPageChange.addRules(
    //       [{conditions: ..., actions: [ new SetIcon(...) ]}]);
    // ). Some of this is tracked in http://crbug.com/415315.
    setIcon(parameters, $Function.bind(function(data) {
      // Fake calling the original function as a constructor.
      $Object.setPrototypeOf(this, nativeSetIcon.prototype);
      $Function.apply(nativeSetIcon, this, [data]);
    }, this));
  };
});
