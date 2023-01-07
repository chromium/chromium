// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const div = parent.document.createElement('div');
  div.setAttribute('id', 'detach-evaluated');
  parent.document.body.appendChild(div);
}

parent.document.querySelector('#injected').remove();
