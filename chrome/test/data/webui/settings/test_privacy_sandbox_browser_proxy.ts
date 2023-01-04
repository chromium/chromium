// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CanonicalTopic, FledgeState, PrivacySandboxBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacySandboxBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxBrowserProxy {
  private fledgeState_: FledgeState;

  constructor() {
    super([
      'getFledgeState',
      'setFledgeJoiningAllowed',
      'getTopicsState',
      'setTopicAllowed',
    ]);

    this.fledgeState_ = {
      joiningSites: ['test-site-one.com'],
      blockedSites: ['test-site-two.com'],
    };
  }

  setFledgeState(fledgeState: FledgeState) {
    this.fledgeState_ = fledgeState;
  }

  getFledgeState() {
    this.methodCalled('getFledgeState');
    return Promise.resolve(this.fledgeState_);
  }

  setFledgeJoiningAllowed(site: string, allowed: boolean) {
    this.methodCalled('setFledgeJoiningAllowed', [site, allowed]);
  }

  getTopicsState() {
    this.methodCalled('getTopicsState');
    return Promise.resolve({
      topTopics:
          [{topicId: 1, taxonomyVersion: 1, displayString: 'test-topic-1'}],
      blockedTopics:
          [{topicId: 2, taxonomyVersion: 1, displayString: 'test-topic-2'}],
    });
  }

  setTopicAllowed(topic: CanonicalTopic, allowed: boolean) {
    this.methodCalled('setTopicAllowed', [topic, allowed]);
  }
}
