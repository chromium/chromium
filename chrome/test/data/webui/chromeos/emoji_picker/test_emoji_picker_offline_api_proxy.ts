// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy, GifSubcategoryData, PaginatedGifResponses, Status, VisualContent} from 'chrome://emoji-picker/emoji_picker.js';

export class TestEmojiPickerApiProxyError extends EmojiPickerApiProxy {
  status: Status = Status.kHttpOk;
  readonly noGifs: PaginatedGifResponses = {
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
      Promise<{status: number, featuredGifs: PaginatedGifResponses}> {
    return Promise.resolve({
      status: this.status,
      featuredGifs: this.noGifs,
    });
  }

  override searchGifs():
      Promise<{status: number, searchGifs: PaginatedGifResponses}> {
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
