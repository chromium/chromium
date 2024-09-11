// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverRemote, AmbientProviderInterface, AmbientTheme, TemperatureUnit, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAmbientProvider extends TestBrowserProxy implements
    AmbientProviderInterface {
  albums: AmbientModeAlbum[] = [
    {
      id: '0',
      checked: false,
      title: '0',
      description: '0',
      numberOfPhotos: 0,
      topicSource: TopicSource.kArtGallery,
      url: {url: 'http://test_url0'},
    },
    {
      id: '1',
      checked: false,
      title: '1',
      description: '1',
      numberOfPhotos: 0,
      topicSource: TopicSource.kArtGallery,
      url: {url: 'http://test_url1'},
    },
    {
      id: '2',
      checked: true,
      title: '2',
      description: '2',
      numberOfPhotos: 0,
      topicSource: TopicSource.kArtGallery,
      url: {url: 'http://test_url2'},
    },
    {
      id: '3',
      checked: true,
      title: '3',
      description: '3',
      numberOfPhotos: 1,
      topicSource: TopicSource.kGooglePhotos,
      url: {url: 'http://test_url3'},
    },
    {
      id: '4',
      checked: true,
      title: '4',
      description: '4',
      numberOfPhotos: 1,
      topicSource: TopicSource.kVideo,
      url: {url: 'http://test_url4'},
    },
    {
      id: '5',
      checked: false,
      title: '5',
      description: '5',
      numberOfPhotos: 1,
      topicSource: TopicSource.kVideo,
      url: {url: 'http://test_url5'},
    },
  ];

  shouldShowBanner: boolean = true;
  geolocationEnabled: boolean = true;
  geolocationIsUserModifiable: boolean = true;

  previews: Url[] = [
    {url: 'http://preview0'},
    {url: 'http://preview1'},
    {url: 'http://preview2'},
    {url: 'http://preview#'},
  ];

  constructor() {
    super([
      'isAmbientModeEnabled',
      'setAmbientObserver',
      'setAmbientModeEnabled',
      'setAmbientTheme',
      'setPageViewed',
      'setScreenSaverDuration',
      'setTopicSource',
      'setTemperatureUnit',
      'setAlbumSelected',
      'startScreenSaverPreview',
      'fetchSettingsAndAlbums',
      'shouldShowTimeOfDayBanner',
      'handleTimeOfDayBannerDismissed',
      'isGeolocationEnabledForSystemServices',
      'isGeolocationUserModifiable',
      'enableGeolocationForSystemServices',
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
  }

  // Test only function to update the ambient observer.
  updateAmbientObserver() {
    this.ambientObserverRemote!.onAmbientModeEnabledChanged(
        /*ambientModeEnabled=*/ true);

    this.ambientObserverRemote!.onAlbumsChanged(this.albums);
    this.ambientObserverRemote!.onAmbientThemeChanged(AmbientTheme.kSlideshow);
    this.ambientObserverRemote!.onTopicSourceChanged(TopicSource.kArtGallery);
    this.ambientObserverRemote!.onTemperatureUnitChanged(
        TemperatureUnit.kFahrenheit);
    this.ambientObserverRemote!.onPreviewsFetched(this.previews);
  }

  setAmbientModeEnabled(ambientModeEnabled: boolean) {
    this.methodCalled('setAmbientModeEnabled', ambientModeEnabled);
  }

  setAmbientTheme(ambientTheme: AmbientTheme) {
    this.methodCalled('setAmbientTheme', ambientTheme);
  }

  setScreenSaverDuration(minutes: number): void {
    this.methodCalled('setScreenSaverDuration', minutes);
  }

  setTopicSource(topicSource: TopicSource) {
    this.methodCalled('setTopicSource', topicSource);
  }

  setTemperatureUnit(temperatureUnit: TemperatureUnit) {
    this.methodCalled('setTemperatureUnit', temperatureUnit);
  }

  setAlbumSelected(id: string, topicSource: TopicSource, selected: boolean) {
    this.methodCalled('setAlbumSelected', id, topicSource, selected);
  }

  setPageViewed() {
    this.methodCalled('setPageViewed');
  }

  startScreenSaverPreview() {
    this.methodCalled('startScreenSaverPreview');
  }

  fetchSettingsAndAlbums() {
    this.methodCalled('fetchSettingsAndAlbums');
  }

  shouldShowTimeOfDayBanner(): Promise<{shouldShowBanner: boolean}> {
    this.methodCalled('shouldShowTimeOfDayBanner');
    return Promise.resolve({shouldShowBanner: this.shouldShowBanner});
  }

  handleTimeOfDayBannerDismissed(): void {
    this.methodCalled('handleTimeOfDayBannerDismissed');
  }

  isGeolocationEnabledForSystemServices():
      Promise<{geolocationEnabled: boolean}> {
    this.methodCalled('isGeolocationEnabledForSystemServices');
    return Promise.resolve({geolocationEnabled: this.geolocationEnabled});
  }

  isGeolocationUserModifiable():
      Promise<{geolocationIsUserModifiable: boolean}> {
    this.methodCalled('isGeolocationUserModifiable');
    return Promise.resolve(
        {geolocationIsUserModifiable: this.geolocationIsUserModifiable});
  }

  enableGeolocationForSystemServices() {
    this.geolocationEnabled = true;
    this.methodCalled('enableGeolocationForSystemServices');
  }
}
