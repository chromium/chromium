// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testSlice() {
  var m = new cr.ui.ArrayDataModel([0, 1, 2]);
  assertArrayEquals([0, 1, 2], m.slice());
  assertArrayEquals([1, 2], m.slice(1));
  assertArrayEquals([1], m.slice(1, 2));
}

function testPush() {
  var m = new cr.ui.ArrayDataModel([0, 1, 2]);

  var count = 0;
  m.addEventListener('splice', function(e) {
    count++;
    assertEquals(3, e.index);
    assertArrayEquals([], e.removed);
    assertArrayEquals([3, 4], e.added);
  });

  assertEquals(5, m.push(3, 4));
  var a = m.slice();
  assertArrayEquals([0, 1, 2, 3, 4], a);

  assertEquals(1, count, 'The splice event should only fire once');
}

function testSplice() {
  function compare(array, args) {
    var m = new cr.ui.ArrayDataModel(array.slice());
    var expected = array.slice();
    var result = expected.splice.apply(expected, args);
    assertArrayEquals(result, m.splice.apply(m, args));
    assertArrayEquals(expected, m.slice());
  }

  compare([1, 2, 3], []);
  compare([1, 2, 3], [0, 0]);
  compare([1, 2, 3], [0, 1]);
  compare([1, 2, 3], [1, 1]);
  compare([1, 2, 3], [0, 3]);
  compare([1, 2, 3], [0, 1, 5]);
  compare([1, 2, 3], [0, 3, 1, 2, 3]);
  compare([1, 2, 3], [5, 3, 1, 2, 3]);
}

function testPermutation() {
  function doTest(sourceArray, spliceArgs) {
    var m = new cr.ui.ArrayDataModel(sourceArray.slice());
    var permutation;
    m.addEventListener('permuted', function(event) {
      permutation = event.permutation;
    });
    m.splice.apply(m, spliceArgs);
    var deleted = 0;
    for (var i = 0; i < sourceArray.length; i++) {
      if (permutation[i] === -1) {
        deleted++;
      } else {
        assertEquals(sourceArray[i], m.item(permutation[i]));
      }
    }
    assertEquals(deleted, spliceArgs[1]);
  }

  doTest([1, 2, 3], [0, 0]);
  doTest([1, 2, 3], [0, 1]);
  doTest([1, 2, 3], [1, 1]);
  doTest([1, 2, 3], [0, 3]);
  doTest([1, 2, 3], [0, 1, 5]);
  doTest([1, 2, 3], [0, 3, 1, 2, 3]);
}

function testUpdateIndexes() {
  var m = new cr.ui.ArrayDataModel([1, 2, 3]);
  var changedIndexes = [];
  m.addEventListener('change', function(event) {
    changedIndexes.push(event.index);
  });
  m.updateIndexes([0, 1, 2]);
  assertArrayEquals([0, 1, 2], changedIndexes);
}

function testReplaceItem() {
  var m = new cr.ui.ArrayDataModel([1, 2, 3]);
  var permutation = null;
  var changeIndex;
  m.addEventListener('permuted', function(event) {
    permutation = event.permutation;
  });
  m.addEventListener('change', function(event) {
    changeIndex = event.index;
  });
  m.replaceItem(2, 4);
  assertEquals(null, permutation);
  assertEquals(1, changeIndex);
}
