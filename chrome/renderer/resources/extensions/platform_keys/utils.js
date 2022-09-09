// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns the intersection of the arrays |a| and |b|, which do not have to be
// sorted.
function intersect(a, b) {
  var result = [];
  for (var i = 0; i < a.length; i++) {
    if (b.indexOf(a[i]) >= 0)
      result.push(a[i]);
  }
  return result;
};

exports.$set('intersect', intersect);
