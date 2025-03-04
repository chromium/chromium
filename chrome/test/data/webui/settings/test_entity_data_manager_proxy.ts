// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {EntityDataManagerProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type AttributeType = chrome.autofillPrivate.AttributeType;
type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

export class TestEntityDataManagerProxy extends TestBrowserProxy implements
    EntityDataManagerProxy {
  private entityInstancesWithLabels_: EntityInstanceWithLabels[] = [];
  private attributeTypes_: AttributeType[] = [];
  private entityInstance_: EntityInstance|null = null;
  private entityTypes_: EntityType[] = [];

  constructor() {
    super([
      'addOrUpdateEntityInstance',
      'removeEntityInstance',
      'loadEntityInstances',
      'getAllAttributeTypesForEntity',
      'getAllEntityTypes',
      'getEntityInstanceByGuid',
    ]);
  }

  setloadEntityInstancesResponse(
      entityInstancesWithLabels: EntityInstanceWithLabels[]): void {
    this.entityInstancesWithLabels_ = entityInstancesWithLabels;
  }

  setGetEntityInstanceByGuidResponse(entityInstance: EntityInstance): void {
    this.entityInstance_ = entityInstance;
  }

  setGetAllEntityTypesResponse(entityTypes: EntityType[]): void {
    this.entityTypes_ = entityTypes;
  }

  setGetAllAttributeTypesForEntityResponse(attributeTypes: AttributeType[]):
      void {
    this.attributeTypes_ = attributeTypes;
  }

  addOrUpdateEntityInstance(entityInstance: EntityInstance): void {
    this.methodCalled(
        'addOrUpdateEntityInstance', structuredClone(entityInstance));
  }

  removeEntityInstance(guid: string): void {
    this.methodCalled('removeEntityInstance', structuredClone(guid));
  }

  loadEntityInstances(): Promise<EntityInstanceWithLabels[]> {
    this.methodCalled('loadEntityInstances');
    return Promise.resolve(structuredClone(this.entityInstancesWithLabels_));
  }

  getEntityInstanceByGuid(guid: string) {
    this.methodCalled('getEntityInstanceByGuid', guid);
    assert(this.entityInstance_!);
    return Promise.resolve(structuredClone(this.entityInstance_));
  }

  getAllEntityTypes() {
    this.methodCalled('getAllEntityTypes');
    return Promise.resolve(structuredClone(this.entityTypes_));
  }

  getAllAttributeTypesForEntity(entityType: number): Promise<AttributeType[]> {
    this.methodCalled('getAllAttributeTypesForEntity', entityType);
    return Promise.resolve(structuredClone(this.attributeTypes_));
  }
}
