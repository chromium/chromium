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
            gif:
                'https://media.tenor.com/TSzcha4Ohi0AAAAC/youre-a-star-star.gif',
            gifpreview:
                'https://media.tenor.com/TSzcha4Ohi0AAAAd/youre-a-star-star.gif',
          },
          previewDims: {
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
        categories: [
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
        featured: this.gifs,
      });
    });
  }

  searchGifs(query, pos) {
    return new Promise((resolve) => {
      resolve({
        gifs: this.gifs,
      });
    });
  }
}
