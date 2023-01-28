// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.usersPrivate
 * for testing.
 */
/**
 * Fake of the chrome.usersPrivate API. Only methods that are called
 * during testing have been implemented.
 *
 * @constructor
 * @implements {UsersPrivate}
 */
export function FakeUsersPrivate() {}

FakeUsersPrivate.prototype = {
  users: [],

  /**
   * @param {User} user
   * @return {!Promise<boolean>}
   */
  addUser(user) {
    this.users.push(user);
    return Promise.resolve(true);
  },

  /**
   * @return {!Promise<User[]>}
   */
  getUsers() {
    return Promise.resolve(this.users);
  },
  /**
   * @param {string} email
   * @return {!Promise<boolean>}
   */
  removeUser(email) {
    this.users = this.users.filter(user => user.email !== email);
    return Promise.resolve(true);
  },
  /**
   * @param {User} user
   * @return {!Promise<boolean>}
   */
  isUserInList(user) {
    return Promise.resolve(this.users.includes(user));
  },
};
