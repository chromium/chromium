// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_
#define CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_

#include "base/macros.h"

// Initializes and destroys the ash::Shell when Ash is running in process.
class AshShellInit {
 public:
  AshShellInit();
  ~AshShellInit();

 private:
  DISALLOW_COPY_AND_ASSIGN(AshShellInit);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_
