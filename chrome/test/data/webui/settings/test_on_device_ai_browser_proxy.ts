// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OnDeviceAiBrowserProxy, OnDeviceAiCapability, OnDeviceAiEnabled} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOnDeviceAiBrowserProxy extends TestBrowserProxy implements
    OnDeviceAiBrowserProxy {
  private onDeviceAiEnabled_: OnDeviceAiEnabled = {
    enabled: true,
    allowedByPolicy: true,
  };
  private capabilities_: OnDeviceAiCapability[] = [];
  private deleteCapabilityResponse_: OnDeviceAiCapability[] = [];

  constructor() {
    super([
      'deleteOnDeviceAiCapability',
      'getOnDeviceAiCapabilities',
      'getOnDeviceAiEnabled',
      'openFeedbackDialog',
      'setOnDeviceAiEnabled',
    ]);
  }

  getOnDeviceAiEnabled() {
    this.methodCalled('getOnDeviceAiEnabled');
    return Promise.resolve(this.onDeviceAiEnabled_);
  }

  setGetOnDeviceAiEnabledResponse(response: OnDeviceAiEnabled) {
    this.onDeviceAiEnabled_ = response;
  }

  setOnDeviceAiEnabled(enabled: boolean) {
    this.methodCalled('setOnDeviceAiEnabled', enabled);
  }

  openFeedbackDialog() {
    this.methodCalled('openFeedbackDialog');
  }

  getOnDeviceAiCapabilities() {
    this.methodCalled('getOnDeviceAiCapabilities');
    return Promise.resolve(this.capabilities_);
  }

  setGetOnDeviceAiCapabilitiesResponse(response: OnDeviceAiCapability[]) {
    this.capabilities_ = response;
  }

  deleteOnDeviceAiCapability(id: number) {
    this.methodCalled('deleteOnDeviceAiCapability', id);
    return Promise.resolve(this.deleteCapabilityResponse_);
  }

  setDeleteOnDeviceAiCapabilityResponse(response: OnDeviceAiCapability[]) {
    this.deleteCapabilityResponse_ = response;
  }
}
