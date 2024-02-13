// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksCommandManagerElement, Command} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {normalizeIterable} from './test_util.js';

export class TestCommandManager {
  private commandManager_: BookmarksCommandManagerElement;
  private lastCommand_: Command|null = null;
  private lastCommandIds_: Set<string>|null = null;

  constructor() {
    this.commandManager_ = document.createElement('bookmarks-command-manager');
    const realHandle = this.commandManager_.handle.bind(this.commandManager_);
    this.commandManager_.handle = (command, itemIds) => {
      this.lastCommand_ = command;
      this.lastCommandIds_ = itemIds;
      realHandle(command, itemIds);
    };
  }

  getCommandManager() {
    return this.commandManager_;
  }

  assertLastCommand(command: Command|null, ids?: string[]) {
    assertEquals(command, this.lastCommand_);
    if (ids) {
      assertDeepEquals(ids, normalizeIterable(this.lastCommandIds_!));
    }
    this.lastCommand_ = null;
    this.lastCommandIds_ = null;
  }

  assertMenuOpenForIds(ids: string[]) {
    assertDeepEquals(
        ids, normalizeIterable(this.commandManager_.getMenuIdsForTesting()));
  }
}
