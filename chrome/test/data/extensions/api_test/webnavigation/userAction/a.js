// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = location.search.slice(1);
var f = document.getElementById('subframe');
f.src = 'http://127.0.0.1:' + port + '/extensions/api_test/webnavigation/userAction/subframe.html';
