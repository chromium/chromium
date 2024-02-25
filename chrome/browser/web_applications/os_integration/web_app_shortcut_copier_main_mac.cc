// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copies files from argv[1] to argv[2]

extern "C" int ChromeWebAppShortcutCopierMain(int argc, char** argv);

int main(int argc, char** argv) {
  return ChromeWebAppShortcutCopierMain(argc, argv);
}
