// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NotificationsTestUtil contains stubs for the global classes and
// variables used by notifications_custom_bindings.js that are not
// available with gtestjs tests.
var apiBridge = {
  registerCustomHook: function() {},
};
var bindingUtil = undefined;

var require = function(library) {
  return {
    lastError: {
      run: function() {}
    },
    sendRequest: {
      sendRequest: function () {}
    },
  }[library];
};

var requireNative = function(library) {
  return {
    notifications_private: {
      GetNotificationImageSizes: function () {
        return {
          scaleFactor: 0,
          icon: { width: 0, height: 0 },
          image: { width: 0, height: 0 },
          buttonIcon: { width: 0, height: 0}
        };
      }
    }
  }[library];
}

var exports = {
  $set: function(k, v) { this.k = v; }
};

var $Array = {
  push: function (ary, val) {
    ary.push(val);
  }
};

var $Function = {
  bind: function (fn, context) {
    return fn.bind(context);
  }
};
