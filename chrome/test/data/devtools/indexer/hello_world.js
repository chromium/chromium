+// Copyright 2017 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.

function hello_world() {
  var div = document.createElement('div');
  document.body.appendChild(div);
  div.textContent = 'Hello, World!';
}

hello_world();
