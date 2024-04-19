// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.usersPrivate
 * for testing.
 */

type User = chrome.usersPrivate.User;
type LoginStatusDict = chrome.usersPrivate.LoginStatusDict;
type UsersPrivateInterface = typeof chrome.usersPrivate;

/**
 * Fake of the chrome.usersPrivate API. Only methods that are called
 * during testing have been implemented.
 */
export class FakeUsersPrivate implements UsersPrivateInterface {
  users: User[] = [];

  setUsersForTesting(users: User[]): void {
    this.users = users;
  }

  addUser(email: string): Promise<boolean> {
    this.users.push({
      email,
      displayEmail: email,
      name: 'Test User',
      isOwner: false,
      isChild: false,
    });
    return Promise.resolve(true);
  }

  getUsers(): Promise<User[]> {
    return Promise.resolve(this.users);
  }

  removeUser(email: string): Promise<boolean> {
    this.users = this.users.filter(user => user.email !== email);
    return Promise.resolve(true);
  }

  isUserInList(email: string): Promise<boolean> {
    const exists = !!this.users.find(user => user.email === email);
    return Promise.resolve(exists);
  }

  isUserListManaged(): Promise<boolean> {
    return Promise.resolve(false);
  }

  getLoginStatus(): Promise<LoginStatusDict[]> {
    const loginStatuses = this.users.map((_user) => {
      return {
        isLoggedIn: true,
        isScreenLocked: false,
      };
    });
    return Promise.resolve(loginStatuses);
  }

  getCurrentUser(): Promise<User> {
    return Promise.resolve(this.users[0]!);
  }
}
