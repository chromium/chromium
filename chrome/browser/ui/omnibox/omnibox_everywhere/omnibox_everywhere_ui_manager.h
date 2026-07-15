// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace omnibox_everywhere {

class OmniboxEverywhereWidgetDelegate;

// Manages the desktop Omnibox Everywhere native window (views::Widget)
// lifecycle and handles switching between different profiles.
class OmniboxEverywhereUIManager : public views::WidgetObserver {
 public:
  OmniboxEverywhereUIManager();
  OmniboxEverywhereUIManager(const OmniboxEverywhereUIManager&) = delete;
  OmniboxEverywhereUIManager& operator=(const OmniboxEverywhereUIManager&) =
      delete;
  ~OmniboxEverywhereUIManager() override;

  // Shows the Omnibox Everywhere widget.
  // |context| is used in testing to attach the widget to a test root window.
  void Show(gfx::NativeWindow context = gfx::NativeWindow());

  // Closes the Omnibox Everywhere widget.
  void Close();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  views::Widget* widget_for_testing() { return widget_.get(); }

 private:
  void CleanUpWidget();
  void OnWidgetClosed(views::Widget::ClosedReason reason);

  // The native window hosting the Omnibox Everywhere UI.
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<OmniboxEverywhereWidgetDelegate> widget_delegate_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
