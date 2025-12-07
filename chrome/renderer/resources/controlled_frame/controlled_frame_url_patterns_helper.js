// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const WebUrlPatternNatives = requireNative('WebUrlPatternNatives');

function convertURLPatternsToMatchPatterns(urlPatternsStrsOrObjs) {
  if (urlPatternsStrsOrObjs === undefined) {
    return undefined;
  }

  let matchPatterns = [];
  for (const urlPatternStrOrObj of urlPatternsStrsOrObjs) {
    matchPatterns = $Array.concat(
        matchPatterns,
        WebUrlPatternNatives.URLPatternToMatchPatterns(
            new URLPattern(urlPatternStrOrObj)),
    );
  }
  return matchPatterns;
}

exports.$set('convertURLPatternsToMatchPatterns',
  convertURLPatternsToMatchPatterns);
