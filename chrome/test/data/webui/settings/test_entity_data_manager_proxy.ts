// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from 'chrome://settings/lazy_load.js';
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
  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;

  constructor() {
    super([
      'addEntityInstancesChangedListener',
      'addOrUpdateEntityInstance',
      'getAllAttributeTypesForEntityTypeName',
      'getAllEntityTypes',
      'getEntityInstanceByGuid',
      'loadEntityInstances',
      'removeEntityInstance',
      'removeEntityInstancesChangedListener',
      'setOptInStatus',
    ]);
  }

  setLoadEntityInstancesResponse(
      entityInstancesWithLabels: EntityInstanceWithLabels[]): void {
    this.entityInstancesWithLabels_ = entityInstancesWithLabels;
  }

  setGetEntityInstanceByGuidResponse(entityInstance: EntityInstance): void {
    this.entityInstance_ = entityInstance;
  }

  setGetAllEntityTypesResponse(entityTypes: EntityType[]): void {
    this.entityTypes_ = entityTypes;
  }

  setGetAllAttributeTypesForEntityTypeNameResponse(
      attributeTypes: AttributeType[]): void {
    this.attributeTypes_ = attributeTypes;
  }

  setOptInStatus(optInStatus: boolean) {
    this.methodCalled('setOptInStatus', optInStatus);
  }

  callEntityInstancesChangedListener(
      entityInstancesWithLabels: EntityInstanceWithLabels[]): void {
    assert(this.entityInstancesChangedListener_);
    this.entityInstancesChangedListener_(entityInstancesWithLabels);
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
    assert(this.entityInstance_);
    return Promise.resolve(structuredClone(this.entityInstance_));
  }

  getAllEntityTypes() {
    this.methodCalled('getAllEntityTypes');
    return Promise.resolve(structuredClone(this.entityTypes_));
  }

  getAllAttributeTypesForEntityTypeName(entityTypeName: number):
      Promise<AttributeType[]> {
    this.methodCalled('getAllAttributeTypesForEntityTypeName', entityTypeName);
    return Promise.resolve(structuredClone(this.attributeTypes_));
  }

  addEntityInstancesChangedListener(listener: EntityInstancesChangedListener) {
    this.methodCalled('addEntityInstancesChangedListener');
    this.entityInstancesChangedListener_ = listener;
  }

  removeEntityInstancesChangedListener(
      _listener: EntityInstancesChangedListener) {
    this.methodCalled('removeEntityInstancesChangedListener');
    this.entityInstancesChangedListener_ = null;
  }
}
