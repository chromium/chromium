// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const fullUrl = new URLSearchParams(window.location.search).get('url');
location.replace(new URL(fullUrl).search.slice(3));
