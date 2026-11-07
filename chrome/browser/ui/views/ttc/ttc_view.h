// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TTC_TTC_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TTC_TTC_VIEW_H_

#include "chrome/browser/ttc/core/states.h"

namespace ttc {

// A piece of TTC UI. Every TtcView draws itself from the session's UiState.
class TtcView {
 public:
  virtual ~TtcView() = default;

  // Makes the view's UI match `state`.
  virtual void ApplyUiState(UiState state) = 0;

  // Makes the view visible.
  virtual void Show() = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_VIEWS_TTC_TTC_VIEW_H_
