// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';

export class TestEmojiPickerApiProxyErrorImpl extends EmojiPickerApiProxyImpl {
  constructor() {
    super();
    this.status = 0;
    this.netError = 1;
    this.httpError = 2;
    this.noGifs = {
      next: '',
      results: [],
    };
  }

  setNetError() {
    this.status = this.netError;
  }

  setHttpError() {
    this.status = this.httpError;
  }

  getCategories() {
    return new Promise((resolve) => {
      resolve({
        gifCategories: [],
      });
    });
  }

  getFeaturedGifs(pos) {
    return new Promise((resolve) => {
      resolve({
        status: this.status,
        featuredGifs: this.noGifs,
      });
    });
  }

  searchGifs(query, pos) {
    return new Promise((resolve) => {
      resolve({
        status: this.status,
        searchGifs: this.noGifs,
      });
    });
  }

  getGifsByIds(ids) {
    return new Promise((resolve) => {
      resolve({
        status: this.status,
        selectedGifs: [],
      });
    });
  }

  copyGifToClipboard(gif) {
    // Fake the backend operation of copying gif to clipboard by doing nothing
  }
}
