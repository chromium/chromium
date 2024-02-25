// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {SharePasswordGroupAvatarElement} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {makeRecipientInfo} from './test_util.js';

const PROFILE_IMG_URL = 'data://image/url';

function createMembers(count: number): chrome.passwordsPrivate.RecipientInfo[] {
  const members = new Array(count);
  for (let i = 0; i < count; i++) {
    members[i] = makeRecipientInfo();
    members[i].profileImageUrl = PROFILE_IMG_URL;
  }
  return members;
}

function assertVisibleImg(img: HTMLImageElement) {
  assertEquals(PROFILE_IMG_URL, img.src);
  assertTrue(isVisible(img));
}

function assertMissingImg(img: HTMLImageElement) {
  assertEquals('', img.src);
  assertFalse(isVisible(img));
}

suite('SharePasswordGroupAvatarTest', function() {
  let element: SharePasswordGroupAvatarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('share-password-group-avatar');
  });

  test('Has correct state with 1 member', async function() {
    element.members = createMembers(1);
    document.body.appendChild(element);
    await flushTasks();

    assertVisibleImg(element.$.firstImg);
    assertMissingImg(element.$.secondImg);
    assertMissingImg(element.$.thirdImg);
    assertMissingImg(element.$.fourthImg);
    assertFalse(isVisible(element.$.more));
  });

  test('Has correct state with 2 member', async function() {
    element.members = createMembers(2);
    document.body.appendChild(element);
    await flushTasks();

    assertVisibleImg(element.$.firstImg);
    assertVisibleImg(element.$.secondImg);
    assertMissingImg(element.$.thirdImg);
    assertMissingImg(element.$.fourthImg);
    assertFalse(isVisible(element.$.more));
  });

  test('Has correct state with 3 member', async function() {
    element.members = createMembers(3);
    document.body.appendChild(element);
    await flushTasks();

    assertVisibleImg(element.$.firstImg);
    assertVisibleImg(element.$.secondImg);
    assertVisibleImg(element.$.thirdImg);
    assertMissingImg(element.$.fourthImg);
    assertFalse(isVisible(element.$.more));
  });

  test('Has correct state with 4 member', async function() {
    element.members = createMembers(4);
    document.body.appendChild(element);
    await flushTasks();

    assertVisibleImg(element.$.firstImg);
    assertVisibleImg(element.$.secondImg);
    assertVisibleImg(element.$.thirdImg);
    assertVisibleImg(element.$.fourthImg);
    assertFalse(isVisible(element.$.more));
  });

  test('Has correct state with 5+ member', async function() {
    element.members = createMembers(5);
    document.body.appendChild(element);
    await flushTasks();

    assertFalse(isVisible(element.$.firstImg));
    assertVisibleImg(element.$.secondImg);
    assertVisibleImg(element.$.thirdImg);
    assertVisibleImg(element.$.fourthImg);
    assertTrue(isVisible(element.$.more));
  });
});
