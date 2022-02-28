// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverRemote, AmbientProviderInterface, TemperatureUnit, TopicSource} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAmbientProvider extends TestBrowserProxy implements
    AmbientProviderInterface {
  constructor() {
    super([
      'isAmbientModeEnabled',
      'setAmbientObserver',
      'setAmbientModeEnabled',
      'setTopicSource',
      'setTemperatureUnit',
      'setAlbumSelected',
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
      const albums: AmbientModeAlbum[] = [
        {
          id: '0',
          checked: false,
          title: '0',
          description: '0',
          numberOfPhotos: 0,
          topicSource: TopicSource.kArtGallery,
          url: {url: 'http://test_url'}
        },
        {
          id: '1',
          checked: false,
          title: '1',
          description: '1',
          numberOfPhotos: 0,
          topicSource: TopicSource.kArtGallery,
          url: {url: 'http://test_url'}
        },
        {
          id: '2',
          checked: true,
          title: '2',
          description: '2',
          numberOfPhotos: 0,
          topicSource: TopicSource.kArtGallery,
          url: {url: 'http://test_url'}
        },
        {
          id: '3',
          checked: false,
          title: '3',
          description: '3',
          numberOfPhotos: 1,
          topicSource: TopicSource.kGooglePhotos,
          url: {url: 'http://test_url'}
        }
      ];
      this.ambientObserverRemote!.onAlbumsChanged(albums);
      this.ambientObserverRemote!.onTopicSourceChanged(TopicSource.kArtGallery);
    }, 10);

    window.setTimeout(() => {
      this.ambientObserverRemote!.onTemperatureUnitChanged(
          TemperatureUnit.kFahrenheit);
    }, 10);
  }

  setAmbientModeEnabled(ambientModeEnabled: boolean) {
    this.methodCalled('setAmbientModeEnabled', ambientModeEnabled);
  }

  setTopicSource(topic_source: TopicSource) {
    this.methodCalled('setTopicSource', topic_source);
  }

  setTemperatureUnit(temperature_unit: TemperatureUnit) {
    this.methodCalled('setTemperatureUnit', temperature_unit);
  }

  setAlbumSelected(id: string, topic_source: TopicSource, selected: boolean) {
    this.methodCalled('setAlbumSelected', id, topic_source, selected);
  }
}
