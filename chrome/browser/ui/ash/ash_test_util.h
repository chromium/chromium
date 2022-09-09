// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_

namespace chromeos {
enum class WindowStateType;
}

namespace aura {
class Window;
}

namespace test {

// The snap window. This will activate the |window|.
void ActivateAndSnapWindow(aura::Window* window,
                           chromeos::WindowStateType type);

}  // namespace test

#endif  // CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
