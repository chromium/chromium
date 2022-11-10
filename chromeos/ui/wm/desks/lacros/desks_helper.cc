// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/desks/desks_helper.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/platform_window/extensions/desk_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "ui/views/widget/widget.h"

namespace {

ui::DeskExtension* GetDeskExtension(aura::Window* window) {
  return views::DesktopWindowTreeHostLacros::From(window->GetHost())
      ->GetDeskExtension();
}

//////////////////////////////////////////////////////////////////////
// DesksHelperLacros implementation:

class DesksHelperLacros : public chromeos::DesksHelper {
 public:
  explicit DesksHelperLacros(aura::Window* window) : window_(window) {}
  DesksHelperLacros(const DesksHelperLacros&) = delete;
  DesksHelperLacros& operator=(const DesksHelperLacros&) = delete;
  ~DesksHelperLacros() override = default;

  // chromeos::DesksHelper:
  bool BelongsToActiveDesk(aura::Window* window) override {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    DCHECK(widget);
    // If the window is on all workspaces or unassigned, we should consider that
    // the window belongs to active desk.
    if (widget->IsVisibleOnAllWorkspaces())
      return true;
    const std::string& workspace = widget->GetWorkspace();
    if (workspace.empty())
      return true;

    int desk_index;
    if (!base::StringToInt(workspace, &desk_index))
      return false;
    return GetActiveDeskIndex() == desk_index;
  }
  int GetActiveDeskIndex() const override {
    return GetDeskExtension(window_)->GetActiveDeskIndex();
  }
  std::u16string GetDeskName(int index) const override {
    return GetDeskExtension(window_)->GetDeskName(index);
  }
  int GetNumberOfDesks() const override {
    return GetDeskExtension(window_)->GetNumberOfDesks();
  }
  void SendToDeskAtIndex(aura::Window* window, int desk_index) override {
    GetDeskExtension(window)->SendToDeskAtIndex(desk_index);
  }

 private:
  raw_ptr<aura::Window> window_;
};

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(DesksHelperLacros,
                                   kDesksHelperLacrosKey,
                                   nullptr)

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(DesksHelperLacros*)

//////////////////////////////////////////////////////////////////////
// DesksHelper implementation:
namespace chromeos {

// static
DesksHelper* DesksHelper::Get(aura::Window* window) {
  DCHECK(window);
  if (auto* desks_helper = window->GetProperty(kDesksHelperLacrosKey))
    return desks_helper;
  return window->SetProperty(kDesksHelperLacrosKey,
                             std::make_unique<DesksHelperLacros>(window));
}

DesksHelper::DesksHelper() = default;

DesksHelper::~DesksHelper() = default;

}  // namespace chromeos
