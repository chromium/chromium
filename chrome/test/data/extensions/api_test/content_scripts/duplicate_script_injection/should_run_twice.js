// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var should_be_injected_twice = document.createElement('div');
should_be_injected_twice.className = 'injected-twice';
document.body.appendChild(should_be_injected_twice);
