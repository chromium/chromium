// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function create(manifest_url) {
  var embed = load_util.embed(manifest_url);
  simple_test.addTestListeners(embed);
  document.body.appendChild(embed);
}

function documentLoaded() {
  create('extension_validation_cache.nmf');
}

document.addEventListener('DOMContentLoaded', documentLoaded);
