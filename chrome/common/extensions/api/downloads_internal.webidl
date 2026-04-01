// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// downloadsInternal
[nodoc]
interface DownloadsInternal {
  // Secretly called when onDeterminingFilename handlers return.
  static undefined determineFilename(long downloadId,
                                     DOMString filename,
                                     DOMString conflict_action);
};

partial interface Browser {
  static attribute DownloadsInternal downloadsInternal;
};
