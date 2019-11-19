// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks/bookmarks.js';
import {normalizeIterable} from 'chrome://test/bookmarks/test_util.js';

export class TestCommandManager {
  constructor() {
    this.commandManager_ = document.createElement('bookmarks-command-manager');
    this.lastCommand_ = null;
    this.lastCommandIds_ = null;
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

  /**
   * @param {Command} command
   * @param {!Array<string>} ids
   */
  assertLastCommand(command, ids) {
    assertEquals(command, this.lastCommand_);
    if (ids) {
      assertDeepEquals(ids, normalizeIterable(this.lastCommandIds_));
    }
    this.lastCommand_ = null;
    this.lastCommandIds_ = null;
  }

  /** @param {!Array<string>} ids */
  assertMenuOpenForIds(ids) {
    assertDeepEquals(ids, normalizeIterable(this.commandManager_.menuIds_));
  }
}
