// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELL_INIT_ASH_SHELL_INIT_H_
#define CHROME_BROWSER_UI_ASH_SHELL_INIT_ASH_SHELL_INIT_H_

// Initializes and destroys the ash::Shell when Ash is running in process.
class AshShellInit {
 public:
  AshShellInit();

  AshShellInit(const AshShellInit&) = delete;
  AshShellInit& operator=(const AshShellInit&) = delete;

  ~AshShellInit();
};

#endif  // CHROME_BROWSER_UI_ASH_SHELL_INIT_ASH_SHELL_INIT_H_
