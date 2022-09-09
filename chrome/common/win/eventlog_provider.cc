// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Force the generation of a .lib file for the .dll since Ninja expects shared
// libraries to generate a .dll and a .lib file.
__declspec(dllexport) bool fn() {
  return true;
}
