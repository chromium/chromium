// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We expect this import to fail as dynamic import is rejected on
// ServiceWorkerGlobalScope.
import('./empty.js');
