// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxyImpl, GifSubcategoryData, Status, TenorGifResponse, VisualContent} from 'chrome://emoji-picker/emoji_picker.js';

export class TestEmojiPickerApiProxyErrorImpl extends EmojiPickerApiProxyImpl {
  status: Status = Status.kHttpOk;
  readonly noGifs: TenorGifResponse = {
    next: '',
    results: [],
  };

  setNetError() {
    this.status = Status.kNetError;
  }

  setHttpError() {
    this.status = Status.kHttpError;
  }

  override getCategories(): Promise<{gifCategories: GifSubcategoryData[]}> {
    return Promise.resolve({
      gifCategories: [],
    });
  }

  override getFeaturedGifs():
      Promise<{status: number, featuredGifs: TenorGifResponse}> {
    return Promise.resolve({
      status: this.status,
      featuredGifs: this.noGifs,
    });
  }

  override searchGifs():
      Promise<{status: number, searchGifs: TenorGifResponse}> {
    return Promise.resolve({
      status: this.status,
      searchGifs: this.noGifs,
    });
  }

  override getGifsByIds():
      Promise<{status: number, selectedGifs: VisualContent[]}> {
    return Promise.resolve({
      status: this.status,
      selectedGifs: [],
    });
  }

  override insertGif() {
    // Fake the backend operation of copying gif to clipboard by doing nothing
  }
}
