// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This needs to be `var` since it's exercising whether this
// script gets injected multiple times, and a `let` would interfere with
// multiple injection -- throwing an error and making it seem like the
// script wasn't injeted multiple times when it was.
// eslint-disable-next-line no-var
var shouldBeInjectedTwice = document.createElement('div');
shouldBeInjectedTwice.className = 'injected-twice';
document.body.appendChild(shouldBeInjectedTwice);
