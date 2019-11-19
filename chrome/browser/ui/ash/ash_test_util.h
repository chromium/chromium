// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_

namespace aura {
class Window;
}

namespace ash {
enum class WindowStateType;
}

namespace test {

// The snap window. This will activate the |window|.
void ActivateAndSnapWindow(aura::Window* window, ash::WindowStateType type);

}  // namespace test

#endif  // CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
