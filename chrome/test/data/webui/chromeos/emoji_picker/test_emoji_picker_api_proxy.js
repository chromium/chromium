// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';

export class TestEmojiPickerApiProxyImpl extends EmojiPickerApiProxyImpl {
  constructor() {
    super();
    // This variable is used to mock the status return value in the actual api
    // proxy.
    // TODO(b/268138636): Change hardcoded value to Status type once tests are
    // migrated to TypeScript.
    this.httpOk = 0;
    // Base64 PNG with dimensions 1 x 1
    this.oneByOneGif =
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=';
    // Base64 PNG with dimensions 1 x 2
    this.oneByTwoGif =
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAACCAQAAAAziH6sAAAADklEQVR42mNk+M/I8B8ABQoCAV5AcKEAAAAASUVORK5CYII=';
    this.initialSet = {
      next: '1',
      results: [
        {
          id: '1',
          contentDescription: 'Left 1',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '2',
          contentDescription: 'Right 1',
          url: {
            full: {
              url: this.oneByOneGif,
            },
            preview: {
              url: this.oneByOneGif,
            },
          },
          previewSize: {
            width: 1,
            height: 1,
          },
        },
        {
          id: '3',
          contentDescription: 'Right 2',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '4',
          contentDescription: 'Left 2',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '5',
          contentDescription: 'Right 3',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '6',
          contentDescription: 'Left 3',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
      ],
    },
    this.followingSet = {
      next: '2',
      results: [
        {
          id: '7',
          contentDescription: 'Right 4',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '8',
          contentDescription: 'Left 4',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '9',
          contentDescription: 'Right 5',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '10',
          contentDescription: 'Left 5',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '11',
          contentDescription: 'Right 6',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
        {
          id: '12',
          contentDescription: 'Left 6',
          url: {
            full: {
              url: this.oneByTwoGif,
            },
            preview: {
              url: this.oneByTwoGif,
            },
          },
          previewSize: {
            width: 1,
            height: 2,
          },
        },
      ],
    },

    this.gifs = {
      trending: this.formatTenorGifResults(this.initialSet, 'Trending'),
      trending1: this.formatTenorGifResults(this.followingSet, 'Trending'),
      face: this.initialSet,
      face1: this.followingSet,
    };
  }

  formatTenorGifResults(gifResults, query) {
    return {
      next: gifResults.next,
      results: gifResults.results.map(
          ({
            id,
            url,
            previewSize,
            contentDescription,
          }) => ({
            id,
            url,
            previewSize,
            contentDescription: query + ' ' + contentDescription,
          })),
    };
  }

  getCategories() {
    return new Promise((resolve) => {
      resolve({
        gifCategories: [
          {name: '#excited'},
          {name: '#angry'},
          {name: '#cry'},
          {name: '#face'},
          {name: '#kiss'},
          {name: '#hugs'},
          {name: '#please'},
          {name: '#dance'},
          {name: '#omg'},
          {name: '#wow'},
        ],
      });
    });
  }

  getFeaturedGifs(pos) {
    const query = pos ? 'trending' + pos : 'trending';
    return new Promise((resolve) => {
      resolve({
        status: this.httpOk,
        featuredGifs: this.gifs[query],
      });
    });
  }

  searchGifs(query, pos) {
    query = (pos ? query + pos : query).replace('#', '');
    return new Promise((resolve) => {
      resolve({
        status: this.httpOk,
        searchGifs: this.gifs[query],
      });
    });
  }

  getGifsByIds(ids) {
    return new Promise((resolve) => {
      resolve({
        status: this.httpOk,
        selectedGifs: [
          {
            id: '1',
            url: {
              full: this.oneByOneGif,
              preview: this.oneByOneGif,
            },
            previewSize: {
              width: 1,
              height: 1,
            },
            contentDescription: 'Right 1 - Left 1',
          },
          {
            id: '2',
            url: {
              full: this.oneByTwoGif,
              preview: this.oneByTwoGif,
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            contentDescription: 'Right 2 - Right 1',
          },
          {
            id: '4',
            url: {
              full: this.oneByTwoGif,
              preview: this.oneByTwoGif,
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            contentDescription: 'Right 3 - Left 2',
          },
        ],
      });
    });
  }

  copyGifToClipboard(gif) {
    // Fake the backend operation of copying gif to clipboard by doing nothing
  }
}
