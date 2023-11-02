// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_THEME_PROFILE_KEY_H_
#define CHROME_BROWSER_UI_VIEWS_THEME_PROFILE_KEY_H_

namespace aura {
class Window;
}

class Profile;

void SetThemeProfileForWindow(aura::Window* window, Profile* profile);
Profile* GetThemeProfileForWindow(aura::Window*);

#endif  // CHROME_BROWSER_UI_VIEWS_THEME_PROFILE_KEY_H_
