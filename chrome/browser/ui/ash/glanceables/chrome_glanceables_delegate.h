// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_

#include "ash/glanceables/glanceables_delegate.h"

// Implements the GlanceablesDelegate interface, allowing access to
// functionality in the //chrome/browser layer.
class ChromeGlanceablesDelegate : public ash::GlanceablesDelegate {
 public:
  ChromeGlanceablesDelegate();
  ChromeGlanceablesDelegate(const ChromeGlanceablesDelegate&) = delete;
  ChromeGlanceablesDelegate& operator=(const ChromeGlanceablesDelegate&) =
      delete;
  ~ChromeGlanceablesDelegate() override;

  // ash::GlanceablesDelegate:
  void RestoreSession() override;
};

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_CHROME_GLANCEABLES_DELEGATE_H_
