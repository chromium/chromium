// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_BORDER_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_BORDER_FACTORY_H_

#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"

namespace ui {
namespace ime {

enum WindowBorderType { Undo, Suggestion };

std::unique_ptr<views::BubbleBorder> GetBorderForWindow(
    WindowBorderType windowType);

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_BORDER_FACTORY_H_
