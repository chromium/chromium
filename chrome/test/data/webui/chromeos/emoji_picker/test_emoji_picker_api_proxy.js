// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';

export class TestEmojiPickerApiProxyImpl extends EmojiPickerApiProxyImpl {
  constructor() {
    super();
    this.gifs = {
      next: '',
      results: [
        {
          url: {
            full:
                'https://media.tenor.com/TSzcha4Ohi0AAAAC/youre-a-star-star.gif',
            preview:
                'https://media.tenor.com/TSzcha4Ohi0AAAAd/youre-a-star-star.gif',
          },
          previewSize: {
            width: 640,
            height: 640,
          },
          contentDescription: 'Youre A Star Star GIF',
        },
      ],
    };
  }

  getCategories() {
    return new Promise((resolve) => {
      resolve({
        gifCategories: [
          {name: '#EXCITED'},
          {name: '#ANGRY'},
          {name: '#CRY'},
          {name: '#CHILL OUT'},
          {name: '#KISS'},
        ],
      });
    });
  }

  getFeaturedGifs(pos) {
    return new Promise((resolve) => {
      resolve({
        featuredGifs: this.gifs,
      });
    });
  }

  searchGifs(query, pos) {
    return new Promise((resolve) => {
      resolve({
        searchGifs: this.gifs,
      });
    });
  }

  getGifsByIds(ids) {
    return new Promise((resolve) => {
      resolve({
        selectedGifs: {
          result: this.gifs.results,
        },
      });
    });
  }
}
