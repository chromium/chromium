// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy, GifSubcategoryData, PaginatedGifResponses, VisualContent} from 'chrome://emoji-picker/emoji_picker.js';

export class TestEmojiPickerApiProxy extends EmojiPickerApiProxy {
  // This variable is used to mock the status return value in the actual api
  // proxy.
  // TODO(b/268138636): Change hardcoded value to Status type once tests are
  // migrated to TypeScript.
  httpOk = 0;
  // Base64 PNG with dimensions 1 x 1
  readonly oneByOneGif =
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=';
  // Base64 PNG with dimensions 1 x 2
  readonly oneByTwoGif =
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAACCAQAAAAziH6sAAAADklEQVR42mNk+M/I8B8ABQoCAV5AcKEAAAAASUVORK5CYII=';


  readonly initialSet: PaginatedGifResponses = {
    next: '1',
    results:
        [
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByOneGif,
              },
            },
            previewSize: {
              width: 1,
              height: 1,
            },
            fullSize: {
              width: 2,
              height: 2,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
            },
          },
        ],
  };

  readonly followingSet: PaginatedGifResponses = {
    next: '2',
    results:
        [
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
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
              previewImage: {
                url: this.oneByTwoGif,
              },
            },
            previewSize: {
              width: 1,
              height: 2,
            },
            fullSize: {
              width: 2,
              height: 4,
            },
          },
        ],
  };

  readonly gifs: {[query: string]: PaginatedGifResponses} = {
    trending: this.formatTenorGifResults(this.initialSet, 'Trending'),
    trending1: this.formatTenorGifResults(this.followingSet, 'Trending'),
  };

  readonly searchResults: {[query: string]: PaginatedGifResponses} = {
    'face': this.initialSet,
    'face1': this.followingSet,
  };

  formatTenorGifResults(gifResults: PaginatedGifResponses, query: string) {
    return {
      next: gifResults.next,
      results: gifResults.results.map(
          (result) => ({
            ...result,
            contentDescription: query + ' ' + result.contentDescription,
          })),
    };
  }

  override getCategories(): Promise<{gifCategories: GifSubcategoryData[]}> {
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

  override getFeaturedGifs(pos?: string):
      Promise<{status: number, featuredGifs: PaginatedGifResponses}> {
    const query = pos ? 'trending' + pos : 'trending';
    return new Promise((resolve) => {
      resolve({
        status: this.httpOk,
        featuredGifs: this.gifs[query] ?? this.gifs['trending']!,
      });
    });
  }

  override searchGifs(query: string, pos?: string):
      Promise<{status: number, searchGifs: PaginatedGifResponses}> {
    query = (pos ? query + pos : query).replace('#', '');
    return Promise.resolve({
      status: this.httpOk,
      searchGifs: this.searchResults[query] ?? this.searchResults['face']!,
    });
  }

  override getGifsByIds():
      Promise<{status: number, selectedGifs: VisualContent[]}> {
    return Promise.resolve({
      status: this.httpOk,
      selectedGifs: [
        {
          id: '1',
          url: {
            full: {url: this.oneByOneGif},
            preview: {url: this.oneByOneGif},
            previewImage: {url: this.oneByOneGif},
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
            full: {url: this.oneByTwoGif},
            preview: {url: this.oneByTwoGif},
            previewImage: {url: this.oneByTwoGif},
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
            full: {url: this.oneByTwoGif},
            preview: {url: this.oneByTwoGif},
            previewImage: {url: this.oneByTwoGif},
          },
          previewSize: {
            width: 1,
            height: 2,
          },
          contentDescription: 'Right 3 - Left 2',
        },
      ],
    });
  }

  override insertGif() {
    // Fake the backend operation of copying gif to clipboard by doing nothing
  }

  override updateHistoryInPrefs() {
    // Fake the backend operation of updating prefs by doing nothing
  }

  override getHistoryFromPrefs() {
    // Fake backend of returning an empty history from prefs.
    return Promise.resolve({history: []});
  }
}
