// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  const port = location.search.substr(1);
  const redirect = `http://127.0.0.1:${port}/server-redirect`;
  const target = `http://127.0.0.1:${port}/not-found`;

  const link = document.createElement('a');
  link.href = `${redirect}?${target}`;
  link.download = 'somefile.txt';
  document.body.appendChild(link);
  link.click();
};
