// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
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
  private requiredAttributeTypes_: AttributeType[] = [];
  private entityInstance_: EntityInstance|null = null;
  private entityTypes_: EntityType[] = [];
  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private optInStatus_: boolean = false;
  private setOptInStatusResponse_: boolean = true;
  private walletOptInStatus_: boolean = false;
  private setWalletablePassDetectionOptInStatusResponse_: boolean = true;
  private authenticateUserBeforeViewingEntityDataResponse_: boolean = true;
  private saveResolver_: PromiseResolver<void>|null = null;
  private autoResolveSave_: boolean = true;

  constructor() {
    super([
      'addEntityInstancesChangedListener',
      'addOrUpdateEntityInstance',
      'authenticateUserBeforeViewingEntityData',
      'getAllAttributeTypesForEntityTypeName',
      'getRequiredAttributeTypesForEntityTypeName',
      'getEntityInstanceByGuid',
      'getOptInStatus',
      'getWalletablePassDetectionOptInStatus',
      'getWritableEntityTypes',
      'loadEntityInstances',
      'removeEntityInstance',
      'removeEntityInstancesChangedListener',
      'setOptInStatus',
      'setWalletablePassDetectionOptInStatus',
      'toggleAutofillAiReauthRequirement',
    ]);
  }

  setLoadEntityInstancesResponse(
      entityInstancesWithLabels: EntityInstanceWithLabels[]): void {
    this.entityInstancesWithLabels_ = entityInstancesWithLabels;
  }

  setAuthenticateUserBeforeViewingEntityDataResponse(success: boolean): void {
    this.authenticateUserBeforeViewingEntityDataResponse_ = success;
  }

  setGetEntityInstanceByGuidResponse(entityInstance: EntityInstance): void {
    this.entityInstance_ = entityInstance;
  }

  setGetWritableEntityTypesResponse(entityTypes: EntityType[]): void {
    this.entityTypes_ = entityTypes;
  }

  setGetAllAttributeTypesForEntityTypeNameResponse(
      attributeTypes: AttributeType[]): void {
    this.attributeTypes_ = attributeTypes;
  }

  setGetRequiredAttributeTypesForEntityTypeNameResponse(
      types: chrome.autofillPrivate.AttributeType[]) {
    this.requiredAttributeTypes_ = types;
  }

  setGetOptInStatusResponse(optInStatus: boolean): void {
    this.optInStatus_ = optInStatus;
  }

  setSetOptInStatusResponse(setOptInStatus: boolean): void {
    this.setOptInStatusResponse_ = setOptInStatus;
  }

  setSetWalletablePassDetectionOptInStatusResponse(success: boolean): void {
    this.setWalletablePassDetectionOptInStatusResponse_ = success;
  }

  callEntityInstancesChangedListener(
      entityInstancesWithLabels: EntityInstanceWithLabels[]): void {
    assert(this.entityInstancesChangedListener_);
    this.entityInstancesChangedListener_(entityInstancesWithLabels);
  }

  /**
   * Helper resolve the pending save operation.
   */
  resolveSave(): void {
    if (this.saveResolver_) {
      this.saveResolver_.resolve();
      this.saveResolver_ = null;
    }
  }

  /**
   * Configures whether the save operation should resolve immediately.
   */
  setAutoResolveSave(autoResolve: boolean) {
    this.autoResolveSave_ = autoResolve;
  }

  addOrUpdateEntityInstance(entityInstance: EntityInstance): Promise<void> {
    this.methodCalled(
        'addOrUpdateEntityInstance', structuredClone(entityInstance));
    if (this.autoResolveSave_) {
      return Promise.resolve();
    }

    this.saveResolver_ = new PromiseResolver();
    return this.saveResolver_.promise;
  }

  removeEntityInstance(guid: string): void {
    this.methodCalled('removeEntityInstance', structuredClone(guid));
  }

  loadEntityInstances(): Promise<EntityInstanceWithLabels[]> {
    this.methodCalled('loadEntityInstances');
    return Promise.resolve(structuredClone(this.entityInstancesWithLabels_));
  }

  getEntityInstanceByGuid(guid: string): Promise<EntityInstance> {
    this.methodCalled('getEntityInstanceByGuid', guid);
    assert(this.entityInstance_);
    return Promise.resolve(structuredClone(this.entityInstance_));
  }

  getWritableEntityTypes(): Promise<EntityType[]> {
    this.methodCalled('getWritableEntityTypes');
    return Promise.resolve(structuredClone(this.entityTypes_));
  }

  getAllAttributeTypesForEntityTypeName(entityTypeName: number):
      Promise<AttributeType[]> {
    this.methodCalled('getAllAttributeTypesForEntityTypeName', entityTypeName);
    return Promise.resolve(structuredClone(this.attributeTypes_));
  }

  getRequiredAttributeTypesForEntityTypeName(entityTypeName: number):
      Promise<chrome.autofillPrivate.AttributeType[]> {
    this.methodCalled(
        'getRequiredAttributeTypesForEntityTypeName', entityTypeName);
    return Promise.resolve(this.requiredAttributeTypes_);
  }

  addEntityInstancesChangedListener(listener: EntityInstancesChangedListener):
      void {
    this.methodCalled('addEntityInstancesChangedListener');
    this.entityInstancesChangedListener_ = listener;
  }

  removeEntityInstancesChangedListener(
      _listener: EntityInstancesChangedListener): void {
    this.methodCalled('removeEntityInstancesChangedListener');
    this.entityInstancesChangedListener_ = null;
  }

  getOptInStatus(): Promise<boolean> {
    this.methodCalled('getOptInStatus');
    return Promise.resolve(this.optInStatus_);
  }

  setOptInStatus(optInStatus: boolean): Promise<boolean> {
    this.methodCalled('setOptInStatus', optInStatus);
    return Promise.resolve(this.setOptInStatusResponse_);
  }

  getWalletablePassDetectionOptInStatus(): Promise<boolean> {
    this.methodCalled('getWalletablePassDetectionOptInStatus');
    return Promise.resolve(this.walletOptInStatus_);
  }

  setWalletablePassDetectionOptInStatus(optedIn: boolean): Promise<boolean> {
    this.methodCalled('setWalletablePassDetectionOptInStatus', optedIn);
    return Promise.resolve(this.setWalletablePassDetectionOptInStatusResponse_);
  }

  authenticateUserBeforeViewingEntityData(): Promise<boolean> {
    this.methodCalled('authenticateUserBeforeViewingEntityData');
    return Promise.resolve(
        this.authenticateUserBeforeViewingEntityDataResponse_);
  }

  toggleAutofillAiReauthRequirement(): void {
    this.methodCalled('toggleAutofillAiReauthRequirement');
  }
}
