// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_TEST_UTILS_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_TEST_UTILS_H_

class Profile;

namespace read_anything::test {

// Makes Reading Mode open in the side panel instead of the immersive overlay,
// which is the default, by setting the user's preferred presentation.
void PreferSidePanel(Profile* profile);

}  // namespace read_anything::test

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_TEST_UTILS_H_
