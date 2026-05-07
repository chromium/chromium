// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CanonicalTopic, FirstLevelTopicsState, FledgeState, PrivacySandboxBrowserProxy, TopicsState} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacySandboxBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxBrowserProxy {
  private fledgeState_: FledgeState = {joiningSites: [], blockedSites: []};
  private topicsState_: TopicsState = {blockedTopics: [], topTopics: []};
  private firstLevelTopicsState_:
      FirstLevelTopicsState = {firstLevelTopics: [], blockedTopics: []};
  private childTopicsCurrentlyAssigned_: CanonicalTopic[] = [];

  constructor() {
    super([
      'getChildTopicsCurrentlyAssigned',
      'getFledgeState',
      'getFirstLevelTopics',
      'getTopicsState',
      'setFledgeJoiningAllowed',
      'setTopicAllowed',
      'topicsToggleChanged',
    ]);
  }

  // Setters for test
  setChildTopics(childTopics: CanonicalTopic[]) {
    this.childTopicsCurrentlyAssigned_ = childTopics;
  }

  setFirstLevelTopicsState(firstLevelTopicsState: FirstLevelTopicsState) {
    this.firstLevelTopicsState_ = firstLevelTopicsState;
  }

  setTestTopicState(topicsState: TopicsState) {
    this.topicsState_ = topicsState;
  }

  setFledgeState(fledgeState: FledgeState) {
    this.fledgeState_ = fledgeState;
  }

  // Test Proxy Functions
  getFledgeState() {
    this.methodCalled('getFledgeState');
    return Promise.resolve(this.fledgeState_);
  }

  setFledgeJoiningAllowed(site: string, allowed: boolean) {
    this.methodCalled('setFledgeJoiningAllowed', [site, allowed]);
  }

  setTopicsState(topicsState: TopicsState) {
    this.topicsState_ = topicsState;
  }

  getTopicsState() {
    this.methodCalled('getTopicsState');
    return Promise.resolve(this.topicsState_);
  }

  setTopicAllowed(topic: CanonicalTopic, allowed: boolean) {
    this.methodCalled('setTopicAllowed', [topic, allowed]);
  }

  topicsToggleChanged(newToggleValue: boolean) {
    this.methodCalled('topicsToggleChanged', [newToggleValue]);
  }

  getFirstLevelTopics() {
    this.methodCalled('getFirstLevelTopics');
    return Promise.resolve(this.firstLevelTopicsState_);
  }

  getChildTopicsCurrentlyAssigned(topic: CanonicalTopic) {
    this.methodCalled(
        'getChildTopicsCurrentlyAssigned', topic.topicId,
        topic.taxonomyVersion);
    return Promise.resolve(this.childTopicsCurrentlyAssigned_.slice());
  }
}
