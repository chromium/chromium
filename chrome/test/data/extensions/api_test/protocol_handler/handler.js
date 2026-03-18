// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const url = new URL(window.location);
const searchParams = new URLSearchParams(url.search);
(window.opener || window.top)
    .postMessage({protocol: searchParams.get('protocol')}, '*');
