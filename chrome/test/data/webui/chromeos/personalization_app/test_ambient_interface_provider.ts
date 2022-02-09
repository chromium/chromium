// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientObserverInterface, AmbientObserverRemote, AmbientProviderInterface, TopicSource} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAmbientProvider extends TestBrowserProxy implements
    AmbientProviderInterface {
  constructor() {
    super([
      'isAmbientModeEnabled',
      'setAmbientObserver',
      'setAmbientModeEnabled',
      'setTopicSource',
    ]);
  }

  ambientObserverRemote: AmbientObserverInterface|null = null;

  isAmbientModeEnabled(): Promise<{enabled: boolean}> {
    this.methodCalled('isAmbientModeEnabled');
    return Promise.resolve({enabled: false});
  }

  setAmbientObserver(remote: AmbientObserverRemote) {
    this.methodCalled('setAmbientObserver', remote);
    this.ambientObserverRemote = remote;
    window.setTimeout(() => {
      this.ambientObserverRemote!.onAmbientModeEnabledChanged(
          /*ambientModeEnabled=*/ true);
    }, 0);

    // Add an arbitrary delay for simulation.
    window.setTimeout(() => {
      this.ambientObserverRemote!.onTopicSourceChanged(TopicSource.kArtGallery);
    }, 10);
  }

  setAmbientModeEnabled(ambientModeEnabled: boolean) {
    this.methodCalled('setAmbientModeEnabled', ambientModeEnabled);
  }

  setTopicSource(topic_source: TopicSource) {
    this.methodCalled('setTopicSource', topic_source);
  }
}
