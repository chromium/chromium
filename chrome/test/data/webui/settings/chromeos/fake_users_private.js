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
  /* #export */ function FakeUsersPrivate() {}

  FakeUsersPrivate.prototype = {
    users: [],

    addUser: function(user) {
      this.users.push(user);
    },

    getUsers: function(callback) {
      return callback(this.users);
    },

    removeUser: function(email, callback) {
      this.users = this.users.filter(user => user.email !== email);
    },

    isUserInList: function(user, callback) {
      callback(this.users.includes(user));
    },
  };

  // #cr_define_end
  return {FakeUsersPrivate: FakeUsersPrivate};
});
