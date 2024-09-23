// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

export class FakeMediaTrack implements MediaStreamTrack {
  contentHint: string = '';
  enabled: boolean = true;
  id: string = Math.random().toString(36);
  kind: string = 'video';  // Or 'audio' as needed
  label: string = '';
  muted: boolean = false;
  readyState: MediaStreamTrackState = 'live';

  private onendedCallback:
      ((this: MediaStreamTrack, ev: Event) => any)|null = null;
  private onmuteCallback:
      ((this: MediaStreamTrack, ev: Event) => any)|null = null;
  private onunmuteCallback:
      ((this: MediaStreamTrack, ev: Event) => any)|null = null;

  stop(): void {
    this.readyState = 'ended';
    if (this.onendedCallback) {
      this.onendedCallback.call(this, new Event('ended'));
    }
  }

  mute(): void {
    this.muted = true;
    if (this.onmuteCallback) {
      this.onmuteCallback.call(this, new Event('mute'));
    }
  }

  unmute(): void {
    this.muted = false;
    if (this.onunmuteCallback) {
      this.onunmuteCallback.call(this, new Event('unmute'));
    }
  }

  set onended(callback: ((this: MediaStreamTrack, ev: Event) => any)|null) {
    this.onendedCallback = callback;
  }

  set onmute(callback: ((this: MediaStreamTrack, ev: Event) => any)|null) {
    this.onmuteCallback = callback;
  }

  set onunmute(callback: ((this: MediaStreamTrack, ev: Event) => any)|null) {
    this.onunmuteCallback = callback;
  }

  applyConstraints(_constraints?: MediaTrackConstraints): Promise<void> {
    throw new Error('Method not implemented.');
  }

  clone(): MediaStreamTrack {
    throw new Error('Method not implemented.');
  }

  getCapabilities(): MediaTrackCapabilities {
    throw new Error('Method not implemented.');
  }

  getConstraints(): MediaTrackConstraints {
    throw new Error('Method not implemented.');
  }

  getSettings(): MediaTrackSettings {
    throw new Error('Method not implemented.');
  }

  addEventListener<K extends keyof MediaStreamTrackEventMap>(
      _type: K,
      _listener:
          (this: MediaStreamTrack, ev: MediaStreamTrackEventMap[K]) => any,
      _options?: boolean|AddEventListenerOptions): void {}

  removeEventListener<K extends keyof MediaStreamTrackEventMap>(
      _type: K,
      _listener:
          (this: MediaStreamTrack, ev: MediaStreamTrackEventMap[K]) => any,
      _options?: boolean|EventListenerOptions): void {}

  dispatchEvent(_event: Event): boolean {
    throw new Error('Method not implemented.');
  }
}

export class FakeMediaStream implements MediaStream {
  private tracks_: MediaStreamTrack[] = [];

  constructor(tracks: FakeMediaTrack[] = []) {
    this.tracks_ = tracks;
  }
  active: boolean = false;
  id: string = '';

  private onaddtrackCallback:
      ((this: MediaStream, ev: MediaStreamTrackEvent) => any)|null = null;
  private onremovetrackCallback:
      ((this: MediaStream, ev: MediaStreamTrackEvent) => any)|null = null;

  set onaddtrack(callback:
                     ((this: MediaStream, ev: MediaStreamTrackEvent) => any)|
                 null) {
    this.onaddtrackCallback = callback;
  }

  addTrack(track: FakeMediaTrack): void {
    this.tracks_.push(track);
    if (this.onaddtrackCallback) {
      this.onaddtrackCallback.call(
          this, new MediaStreamTrackEvent('addtrack', {track}));
    }
  }

  set onremovetrack(callback:
                        ((this: MediaStream, ev: MediaStreamTrackEvent) => any)|
                    null) {
    this.onremovetrackCallback = callback;
  }

  removeTrack(track: FakeMediaTrack): void {
    const index = this.tracks_.indexOf(track);
    if (index > -1) {
      this.tracks_.splice(index, 1);
      if (this.onremovetrackCallback) {
        this.onremovetrackCallback.call(
            this, new MediaStreamTrackEvent('removetrack', {track}));
      }
    }
  }

  clone(): MediaStream {
    throw new Error('Method not implemented.');
  }

  stop(): void {
    throw new Error('Method not implemented.');
  }

  addEventListener<K extends keyof MediaStreamEventMap>(
      type: K, listener: (this: MediaStream, ev: MediaStreamEventMap[K]) => any,
      options?: boolean|AddEventListenerOptions|undefined): void;
  addEventListener(
      type: string, listener: EventListenerOrEventListenerObject,
      options?: boolean|AddEventListenerOptions|undefined): void;
  addEventListener(_type: unknown, _listener: unknown, _options?: unknown):
      void {
    throw new Error('Method not implemented.');
  }
  removeEventListener<K extends keyof MediaStreamEventMap>(
      type: K, listener: (this: MediaStream, ev: MediaStreamEventMap[K]) => any,
      options?: boolean|EventListenerOptions|undefined): void;
  removeEventListener(
      type: string, listener: EventListenerOrEventListenerObject,
      options?: boolean|EventListenerOptions|undefined): void;
  removeEventListener(_type: unknown, _listener: unknown, _options?: unknown):
      void {
    throw new Error('Method not implemented.');
  }

  dispatchEvent(_event: Event): boolean {
    throw new Error('Method not implemented.');
  }

  getAudioTracks(): MediaStreamTrack[] {
    return this.tracks_.filter(track => track.kind === 'audio');
  }

  getVideoTracks(): MediaStreamTrack[] {
    return this.tracks_.filter(track => track.kind === 'video');
  }

  getTracks(): MediaStreamTrack[] {
    return this.tracks_;
  }

  getTrackById(trackId: string): MediaStreamTrack {
    const track = this.tracks_.find(track => track.id === trackId);
    if (!track) {
      throw new Error('Track not found');
    }
    return track;
  }
}

export class FakeMediaDevices implements MediaDevices {
  isStreamingUserFacingCamera: boolean = true;
  private devices_: MediaDeviceInfo[] = [];
  private deviceChangeListener_: EventListener|null = null;
  private enumerateDevicesResolver_: Function|null = null;
  private getMediaDevicesResolver_: Function|null = null;
  private getMediaDevicesRejectResolver_: Function|null = null;
  private stream_: MediaStream|null = null;
  private shouldUserMediaRequestFail_: boolean = false;

  addEventListener(_type: string, listener: EventListener): void {
    this.deviceChangeListener_ = listener;
  }

  enumerateDevices(): Promise<MediaDeviceInfo[]> {
    return new Promise((res, _rej) => {
      this.enumerateDevicesResolver_ = res;
    });
  }

  resolveEnumerateDevices(callback: Function): void {
    assertTrue(
        !!this.enumerateDevicesResolver_, 'enumerateDevices was not called');
    this.enumerateDevicesResolver_(this.devices_);
    callback();
  }

  getSupportedConstraints(): MediaTrackSupportedConstraints {
    return {
      whiteBalanceMode: false,
      exposureMode: false,
      focusMode: false,
      pointsOfInterest: false,

      exposureCompensation: false,
      colorTemperature: false,
      iso: false,

      brightness: false,
      contrast: false,
      saturation: false,
      sharpness: false,
      focusDistance: false,
      zoom: false,
      torch: false,
    };
  }

  getDisplayMedia(): Promise<MediaStream> {
    return Promise.resolve(new MediaStream());
  }

  getUserMedia(constraints: any): Promise<MediaStream> {
    this.isStreamingUserFacingCamera = constraints.video.facingMode === 'user';
    return new Promise((res, rej) => {
      this.getMediaDevicesResolver_ = res;
      this.getMediaDevicesRejectResolver_ = rej;
    });
  }

  /**
   * Resolves promise returned from getUserMedia().
   */
  resolveGetUserMedia(): void {
    assertTrue(
        !!this.getMediaDevicesResolver_ &&
            !!this.getMediaDevicesRejectResolver_,
        'getUserMedia was not called');

    if (this.shouldUserMediaRequestFail_ && this.stream_) {
      this.getMediaDevicesRejectResolver_!
          ('Failed to create stream, a stream currently exist');
    }

    const track = new FakeMediaTrack();

    track.onended = () => {
      if (this.stream_ && !this.shouldUserMediaRequestFail_ &&
          this.stream_.getTracks().every(t => t.readyState === 'ended')) {
        this.stream_ = null;
      }
    };

    this.stream_ = new FakeMediaStream([track]);
    this.getMediaDevicesResolver_(this.stream_);
  }

  removeEventListener(): void {}

  /**
   * Adds a video input device to the list of media devices.
   */
  addDevice(): void {
    const device = {
      deviceId: '',
      kind: 'videoinput',
      label: '',
      groupId: '',
    } as MediaDeviceInfo;
    // https://w3c.github.io/mediacapture-main/#dom-mediadeviceinfo
    (device as any).__proto__ = MediaDeviceInfo.prototype;
    this.devices_.push(device);
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_(new Event('addDevice'));
    }
  }

  /**
   * Removes the most recently added media device from the list of media
   * devices.
   */
  removeDevice(): void {
    this.devices_.pop();
    if (this.devices_.length <= 1) {
      this.isStreamingUserFacingCamera = true;
    }
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_(new Event('removeDevice'));
    }
  }

  ondevicechange(): void {}

  setShouldUserMediaRequestFail(shouldUserMediaRequestFail: boolean) {
    this.shouldUserMediaRequestFail_ = shouldUserMediaRequestFail;
  }

  dispatchEvent(_event: Event): boolean {
    return false;
  }
}
