// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {EntityDataManagerProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type EntityInstance = chrome.autofillPrivate.EntityInstance;

export class TestEntityDataManagerProxy extends TestBrowserProxy implements
    EntityDataManagerProxy {
  private entities_: EntityInstance[] = [];

  constructor() {
    super([
      'addOrUpdateEntityInstance',
      'removeEntityInstance',
      'loadEntityInstances',
    ]);
  }

  setloadEntityInstancesResponse(entities: EntityInstance[]): void {
    this.entities_ = entities;
  }

  addOrUpdateEntityInstance(entityInstance: EntityInstance): void {
    this.methodCalled(
        'addOrUpdateEntityInstance', structuredClone(entityInstance));
  }

  removeEntityInstance(guid: string): void {
    this.methodCalled('removeEntityInstance', structuredClone(guid));
  }

  loadEntityInstances(): Promise<EntityInstance[]> {
    this.methodCalled('loadEntityInstances');
    return Promise.resolve(structuredClone(this.entities_));
  }
}
