// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Increments the file index of a given file name to avoid name conflicts.
 * @param {string} name File name.
 * @return {string} File name with incremented index.
 */
export function incrementFileName(name) {
  let base = '';
  let ext = '';
  let idx = 0;
  let match = name.match(/^([^.]+)(\..+)?$/);
  if (match) {
    base = match[1];
    ext = match[2];
    match = base.match(/ \((\d+)\)$/);
    if (match) {
      base = base.substring(0, match.index);
      idx = parseInt(match[1], 10);
    }
  }
  return base + ' (' + (idx + 1) + ')' + ext;
}
