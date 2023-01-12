// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function fetchResource() {
  let result;
  await fetch('/extensions/resource_timing/24.png')
    .then(() => { result = true; }).catch(() => { result = false });
  return result;
}
