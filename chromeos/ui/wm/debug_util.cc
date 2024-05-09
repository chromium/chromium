// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/debug_util.h"

#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace chromeos::wm {
namespace {

void PrintWindowHierarchy(const aura::Window* active_window,
                          const aura::Window* focused_window,
                          const aura::Window* capture_window,
                          aura::Window* window,
                          int indent,
                          bool scrub_data,
                          GetChildrenCallback children_callback,
                          std::vector<std::string>* out_window_titles,
                          std::ostringstream* out) {
  *out << std::string(indent, ' ');
  *out << " [window]";
  window->GetDebugInfo(active_window, focused_window, capture_window, out);
  if (base::Contains(window->GetAllPropertyKeys(),
                     chromeos::kWindowStateTypeKey)) {
    *out << " state=" << window->GetProperty(chromeos::kWindowStateTypeKey);
  }

  std::u16string title(window->GetTitle());
  if (!title.empty()) {
    if (!scrub_data) {
      *out << " title=\"" << title << "\"";
    }
    out_window_titles->push_back(base::UTF16ToUTF8(title));
  }

  std::string* tree_id = window->GetProperty(ui::kChildAXTreeID);
  if (tree_id) {
    *out << " ax_tree_id=" << *tree_id;
  }

  *out << std::endl;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    *out << std::string(indent + 3, ' ');
    *out << " [widget]";
    views::PrintWidgetInformation(*widget, /*detailed*/ false, out);
  }

  std::vector<raw_ptr<aura::Window, VectorExperimental>> children =
      children_callback.is_null() ? window->children()
                                  : children_callback.Run(window);
  for (aura::Window* child : children) {
    PrintWindowHierarchy(active_window, focused_window, capture_window, child,
                         indent + 3, scrub_data, children_callback,
                         out_window_titles, out);
  }
}

}  // namespace

std::vector<std::string> PrintWindowHierarchy(
    aura::Window::Windows roots,
    bool scrub_data,
    std::ostringstream* out,
    GetChildrenCallback children_callback) {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/41496823): Make ActiveClient and FocusClient return the same
  // window across all instances of the clients on Lacros.
  aura::Window* root0 = roots[0];
  aura::Window* active_window =
      ::wm::GetActivationClient(root0)->GetActiveWindow();
  aura::Window* focused_window =
      aura::client::GetFocusClient(root0)->GetFocusedWindow();
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root0)->GetCaptureWindow();
#endif

  std::vector<std::string> window_titles;
  for (size_t i = 0; i < roots.size(); ++i) {
    *out << "RootWindow " << i << ":\n";
    aura::Window* root = roots[i];
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    aura::Window* active_window =
        ::wm::GetActivationClient(root)->GetActiveWindow();
    aura::Window* focused_window =
        aura::client::GetFocusClient(root)->GetFocusedWindow();
    aura::Window* capture_window =
        aura::client::GetCaptureClient(root)->GetCaptureWindow();
#else
    // These windows must be the same across root windows.
    DCHECK_EQ(active_window,
              ::wm::GetActivationClient(root)->GetActiveWindow());
    DCHECK_EQ(focused_window,
              aura::client::GetFocusClient(root)->GetFocusedWindow());
    DCHECK_EQ(capture_window,
              aura::client::GetCaptureClient(root)->GetCaptureWindow());
#endif

    PrintWindowHierarchy(active_window, focused_window, capture_window, root, 0,
                         scrub_data, children_callback, &window_titles, out);
  }
  return window_titles;
}

}  // namespace chromeos::wm
