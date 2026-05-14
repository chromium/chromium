// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fake MediaQueryList used in mocking response of |window.matchMedia|.
export class FakeMediaQueryList extends EventTarget implements MediaQueryList {
  private matches_: boolean = false;
  private media_: string;

  constructor(media: string) {
    super();
    this.media_ = media;
  }

  addListener(_listener: (e: MediaQueryListEvent) => void) {}
  removeListener(_listener: (e: MediaQueryListEvent) => void) {}
  onchange(_e: MediaQueryListEvent) {}

  private notifyListeners_() {
    const event = new MediaQueryListEvent(
        'change', {media: this.media_, matches: this.matches_});
    this.dispatchEvent(event);
  }

  get media(): string {
    return this.media_;
  }

  get matches(): boolean {
    return this.matches_;
  }

  set matches(matches: boolean) {
    if (this.matches_ !== matches) {
      this.matches_ = matches;
      this.notifyListeners_();
    }
  }
}
