// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NotificationsTestUtil contains stubs for the global classes and
// variables used by notifications_custom_bindings.js that are not
// available with gtestjs tests.

Object.assign(window, {
  apiBridge: {
    registerCustomHook: function() {},
  },
  bindingUtil: undefined,

  require: function(library) {
    return {
      lastError: {run: function() {}},
      sendRequest: {sendRequest: function() {}},
    }[library];
  },

  requireNative: function(library) {
    return {
      notifications_private: {
        GetNotificationImageSizes: function() {
          return {
            scaleFactor: 0,
            icon: {width: 0, height: 0},
            image: {width: 0, height: 0},
            buttonIcon: {width: 0, height: 0},
          };
        },
      },
    }[library];
  },

  exports: {
    $set: function(k, v) {
      this.k = v;
    },
  },

  $Array: {
    push: function(ary, val) {
      ary.push(val);
    },
  },

  $Function: {
    bind: function(fn, context) {
      return fn.bind(context);
    },
  },
});
