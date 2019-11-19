// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Limited externs used by webui testing from http://mock4js.sourceforge.net/.
 * Mock4JS is not closure-annotated and unmaintained.
 * TODO(crbug/844820): Eliminate/replace usage of mock4js and delete this file.
 */

function mock(klass) {}

const Mock4JS = {
  verifyAllMocks: function() {},
  addMockSupport: function(exports) {},
};

class Mock {
  proxy() {}
  expects(expectedCallCount) {}
  stubs() {}
  verify() {}
}
