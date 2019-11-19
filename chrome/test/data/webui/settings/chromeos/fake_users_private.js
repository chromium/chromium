// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.usersPrivate
 * for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.usersPrivate API. Only methods that are called
   * during testing have been implemented.
   *
   * @constructor
   * @implements {UsersPrivate}
   */
  function FakeUsersPrivate() {
  }

  FakeUsersPrivate.prototype = {
    users: [],

    addWhitelistedUser: function(user) {
      this.users.push(user);
    },

    isWhitelistedUser: function(user, callback) {
      callback(this.users.includes(user));
    },
  };

  return {FakeUsersPrivate: FakeUsersPrivate};
});