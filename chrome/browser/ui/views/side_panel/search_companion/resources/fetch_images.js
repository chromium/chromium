// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  const output = {};
  output['images'] = 'Document Title: ' + document.title +
      ' Images On Page: ' + document.getElementsByTagName('img').length + '\n';
  const imageTags = [].slice.call(document.getElementsByTagName("img"));
  // Sort by image size
  imageTags.sort(function(a, b){return (b.clientWidth * b.clientHeight) - (a.clientWidth * a.clientHeight);});
  for (let i = 0; i < imageTags.length; i++) {
    // Skip if image is not in viewport
    const curImage = imageTags[i];
    const rect = curImage.getBoundingClientRect();
    if (!(rect.top >= 0 &&
        rect.left >= 0 &&
        rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
        rect.right <= (window.innerWidth || document.documentElement.clientWidth))) {
      continue;
    }
    // Ensure image is at least 100x100 or equivalent to remove icons.
    if (curImage.clientWidth * curImage.clientHeight < 10000) {
      continue;
    }
    const label = ' Image ' + i + ' Src: ' + curImage.src +
        ' Width: ' + curImage.clientWidth +
        ' Height: ' + curImage.clientHeight + '\n';
    output['images'] += label;
  }

  return JSON.stringify(output);
})();