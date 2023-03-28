// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var textinput_helper = {
  retrieveElementCoordinate: function(id) {
    var ele = document.getElementById(id);
    var coordinate = Math.floor(ele.offsetLeft) + ',' +
        Math.floor(ele.offsetTop) + ',' +
        Math.ceil(ele.offsetWidth) + ',' +
        Math.ceil(ele.offsetHeight);
    return coordinate;
  }
};
